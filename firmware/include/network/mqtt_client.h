#pragma once

#include "config/board_config.h"

#include <cstddef>
#include <cstdint>

#if APP_ENABLE_MQTT
#include <PubSubClient.h>
#include <WiFi.h>
#endif

namespace ppgfw {

class MqttClient {
public:
    MqttClient();
    void begin();
    void loop(uint32_t now_ms);
    bool publish(const char* payload);
    bool connected();

private:
#if APP_ENABLE_MQTT
    WiFiClient wifi_client_{};
    PubSubClient mqtt_client_;
#endif
    uint32_t next_reconnect_ms_{};
};

}
