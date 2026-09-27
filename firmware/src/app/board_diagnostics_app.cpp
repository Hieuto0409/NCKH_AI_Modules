#include "app/board_diagnostics_app.h"
#if defined(PPGFW_BOARD_DIAGNOSTICS)
#include "acquisition/ecg_acquisition.h"
#include "acquisition/ppg_acquisition.h"
#include "drivers/esp32_adc_backend.h"
#include "config/board_config.h"
#include "config/pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <algorithm>
namespace ppgfw {
namespace {
Max30102Driver driver;
Esp32AdcBackend adc;
PpgSampleQueue pq;
EcgSampleQueue eq;
PpgAcquisition ppg(driver,pq);
EcgAcquisition ecg(adc,eq);
int modeRegister() {
    Wire.beginTransmission(0x57);Wire.write(0x09);
    if(Wire.endTransmission(false)!=0 || Wire.requestFrom(uint8_t(0x57),uint8_t(1))!=1)return -1;
    return Wire.read();
}
void capture(const char* label) {
    const bool p=ppg.setActive(true),e=ecg.setActive(true);
    ppg.resetWindowDiagnostics();ecg.resetWindowDiagnostics();
    uint32_t pn=0,en=0,redmin=UINT32_MAX,redmax=0,irmin=UINT32_MAX,irmax=0;
    int emin=4096,emax=0;uint64_t last=0,mindt=UINT64_MAX,maxdt=0;
    uint32_t reversals=0; const uint64_t start=TimestampService::nowUs();
    uint64_t next=start;
    while(TimestampService::nowUs()-start<10000000ULL) {
        const auto now=TimestampService::nowUs();
        if(now>=next){ppg.poll();next=now+10000;}
        PpgSample ps;while(pq.pop(ps)){
            ++pn;redmin=std::min(redmin,ps.red);redmax=std::max(redmax,ps.red);
            irmin=std::min(irmin,ps.ir);irmax=std::max(irmax,ps.ir);
        }
        EcgSample es;while(eq.pop(es)){
            ++en;emin=std::min(emin,int(es.raw));emax=std::max(emax,int(es.raw));
            if(last){if(es.timestamp_us<=last)++reversals;
                else {const auto dt=es.timestamp_us-last;mindt=std::min(mindt,dt);maxdt=std::max(maxdt,dt);}}
            last=es.timestamp_us;
        }
        delay(1);
    }
    const auto elapsed=TimestampService::nowUs()-start;
    const bool ps=ppg.setActive(false),es=ecg.setActive(false);
    const auto pd=ppg.diagnostics(),ed=ecg.diagnostics();
    Serial.printf("CAPTURE %s begin=%u/%u stop=%u/%u elapsed_us=%llu ppg=%lu ecg=%lu drops=%lu/%lu overflow=%lu qdrop=%lu/%lu lead_off=%lu ecg_dt_us=%llu:%llu reversals=%lu raw_ecg=%d:%d red=%lu:%lu ir=%lu:%lu max_mode=%d\n",
        label,p,e,ps,es,elapsed,pn,en,pd.dropped_samples,ed.dropped_samples,pd.overflow_count,
        pd.queue_drop_count,ed.queue_drop_count,ed.lead_off_count,mindt,maxdt,reversals,
        emin,emax,redmin,redmax,irmin,irmax,modeRegister());
}
}
void BoardDiagnosticsApp::begin() {
    Serial.begin(config::kSerialBaud);delay(250);
    Serial.println("*** BOARD DIAGNOSTICS - NOT A BODY MEASUREMENT ***");
    wifi_mode_t mode=WIFI_MODE_NULL;const auto wifi=esp_wifi_get_mode(&mode);
    Serial.printf("BOARD flash=%lu psram=%lu heap=%lu wifi_err=%d wifi_mode=%u\n",
        ESP.getFlashChipSize(),ESP.getPsramSize(),ESP.getFreeHeap(),int(wifi),unsigned(mode));
    auto* ram=static_cast<uint32_t*>(heap_caps_malloc(32768,MALLOC_CAP_SPIRAM));
    bool ram_ok=ram!=nullptr;
    if(ram){for(unsigned i=0;i<8192;++i)ram[i]=i^0xa55a5aa5U;
        for(unsigned i=0;i<8192;++i)if(ram[i]!=(i^0xa55a5aa5U))ram_ok=false;heap_caps_free(ram);}
    Serial.printf("PSRAM pattern_32KiB=%s\n",ram_ok?"PASS":"FAIL");
    const bool p=ppg.begin(),e=ecg.begin();
    Serial.printf("DRIVERS ppg=%u ecg=%u\n",p,e);
    ppg.setActive(false);ecg.setActive(false);
    for(uint8_t address:{uint8_t(0x57),uint8_t(0x68)}){
        Wire.beginTransmission(address);Serial.printf("I2C address=0x%02x result=%u\n",address,Wire.endTransmission());}
    if(p && e){
        capture("first");const auto before=ecg.diagnostics().received_samples;
        delay(3000);Serial.printf("PAUSE ecg_added=%lu max_mode=%d\n",ecg.diagnostics().received_samples-before,modeRegister());
        Serial.flush();const auto start=TimestampService::nowUs();
        const auto timer=esp_sleep_enable_timer_wakeup(1000000);
        const auto sleep=timer==ESP_OK?esp_light_sleep_start():timer;
        Serial.printf("LIGHT_SLEEP err=%d elapsed_us=%llu cause=%d\n",int(sleep),TimestampService::nowUs()-start,int(esp_sleep_get_wakeup_cause()));
        capture("after_sleep");
    }
    Serial.printf("MEMORY heap=%lu min_heap=%lu stack_free=%u\n",ESP.getFreeHeap(),ESP.getMinFreeHeap(),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    Serial.println("*** BOARD DIAGNOSTICS COMPLETE ***");
}
void BoardDiagnosticsApp::loop(){delay(1000);}
}
#endif
