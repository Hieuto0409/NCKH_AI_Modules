#include "app/network_diagnostics_app.h"
#if defined(PPGFW_NETWORK_DIAGNOSTICS)
#include "app/measurement_state_machine.h"
#include "network/mqtt_client.h"
#include "drivers/max30102_driver.h"
#include "ui/oled_ui.h"
#include "config/network_config.h"
#include "config/versions.h"
#include <Arduino.h>
#include <ctime>
namespace ppgfw {
namespace {
MqttClient client;
Max30102Driver sensor;
OledUi oled;
ResultSnapshot empty_result;
constexpr const char* kProbeId = "ppgfw-board-20260927-01";
}
void NetworkDiagnosticsApp::begin() {
    Serial.begin(config::kSerialBaud); delay(250);
    Serial.println("*** NETWORK DIAGNOSTICS - NO BODY MEASUREMENT ***");
    const bool ppg = sensor.begin();
    const bool stopped = sensor.shutdown();
    oled.begin(); client.begin();
    Serial.printf("NETTEST sensors ppg=%u shutdown=%u ecg_task=not_started\n", ppg, stopped);
    // Explicit network test only. Production never scans or sends at boot.
    WiFi.mode(WIFI_STA);
    const int n = WiFi.scanNetworks(false, false, false, 150);
    bool found = false;
    for (int i=0; i<n; ++i) if (WiFi.SSID(i) == config::network::kWifiSsid) {
        found = true;
        Serial.printf("NETTEST target_visible=1 channel=%ld rssi=%ld\n", WiFi.channel(i), WiFi.RSSI(i));
    }
    WiFi.scanDelete(); WiFi.mode(WIFI_OFF);
    if (!found) Serial.println("NETTEST target_visible=0");
    char payload[256];
    snprintf(payload,sizeof(payload),
        "{\"integration_test\":true,\"test_origin\":\"esp32\",\"tb_board_probe_id\":\"%s\",\"firmware_target\":\"%s\"}",
        kProbeId, config::kFirmwareVersion);
    const bool queued = client.publish(payload);
    client.loop(millis(),false);
    Serial.printf("NETTEST acquisition_gate queued=%u pending=%u radio=%u mode=%u\n",
        queued,unsigned(client.pending()),client.radioActive(),unsigned(WiFi.getMode()));
    const bool retry = client.retryPending();
    const uint32_t start = millis();
    auto last_stage = UploadStage::Offline;
    uint32_t last_print=0;
    while (retry && millis()-start < 35000) {
        client.loop(millis(),true);
        const auto status=client.uploadStatus();
        if(status.stage!=last_stage || millis()-last_print>=1000) {
            last_stage=status.stage;last_print=millis();
            Serial.printf("NETTEST stage=%u wifi=%u pending=%u ack=%lu http=%d time_ok=%u elapsed_ms=%lu\n",
                unsigned(status.stage),unsigned(WiFi.status()),unsigned(status.pending),
                status.acknowledged,client.lastHttpStatus(),time(nullptr)>=1704067200,millis()-start);
            oled.render(MeasurementState::Idle,empty_result,0,ppg,false,true,0,status);
        }
        if(client.acknowledged() || (!client.radioActive() && status.stage==UploadStage::Deferred)) break;
        delay(10);
    }
    client.suspend();
    Serial.printf("NETTEST result probe_id=%s ack=%lu http=%d radio=%u mode=%u heap=%lu min_heap=%lu\n",
        kProbeId,client.acknowledged(),client.lastHttpStatus(),client.radioActive(),unsigned(WiFi.getMode()),
        ESP.getFreeHeap(),ESP.getMinFreeHeap());
    // Simulate starting acquisition while connecting. This synthetic packet stays local.
    if(client.acknowledged()) {
        client.publish("{\"cancelled_network_probe\":true}");
        client.loop(millis(),true);client.loop(millis(),false);
        Serial.printf("NETTEST cancel pending=%u ack=%lu radio=%u mode=%u\n",
            unsigned(client.pending()),client.acknowledged(),client.radioActive(),unsigned(WiFi.getMode()));
    }
    oled.setSleeping(true);
    Serial.println("*** NETWORK DIAGNOSTICS COMPLETE ***");
}
void NetworkDiagnosticsApp::loop() { delay(1000); }
}
#endif
