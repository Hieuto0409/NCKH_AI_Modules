#include "network/mqtt_client.h"

#include "config/network_config.h"

#if APP_ENABLE_MQTT
#include <Arduino.h>
#include <cstring>
#endif

namespace ppgfw {

MqttClient::MqttClient()
#if APP_ENABLE_MQTT
    : mqtt_client_(wifi_client_)
#endif
{
}

void MqttClient::begin() {
#if APP_ENABLE_MQTT
    if (std::strlen(config::network::kWifiSsid) == 0 ||
        std::strlen(config::network::kMqttHost) == 0) {
        return;
    }
    if (!mqtt_client_.setBufferSize(2048)) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(config::network::kWifiSsid, config::network::kWifiPassword);
    mqtt_client_.setServer(config::network::kMqttHost, config::network::kMqttPort);
    mqtt_client_.setSocketTimeout(1);
#endif
}

void MqttClient::loop(uint32_t now_ms) {
#if APP_ENABLE_MQTT
    if (std::strlen(config::network::kWifiSsid) == 0 ||
        std::strlen(config::network::kMqttHost) == 0 || WiFi.status() != WL_CONNECTED) {
        return;
    }
    if (!mqtt_client_.connected() && now_ms >= next_reconnect_ms_) {
        char client_id[32]{};
        std::snprintf(client_id, sizeof(client_id), "ppgfw-%08lx",
                      static_cast<unsigned long>(ESP.getEfuseMac()));
        if (!mqtt_client_.connect(client_id)) {
            next_reconnect_ms_ = now_ms + config::network::kReconnectBackoffMs;
            return;
        }
    }
    mqtt_client_.loop();
#else
    (void)now_ms;
#endif
}

bool MqttClient::publish(const char* payload) {
#if APP_ENABLE_MQTT
    return payload != nullptr && mqtt_client_.connected() &&
           mqtt_client_.publish(config::network::kMqttTopic, payload, false);
#else
    (void)payload;
    return false;
#endif
}

bool MqttClient::connected() {
#if APP_ENABLE_MQTT
    return mqtt_client_.connected();
#else
    return false;
#endif
}

}
