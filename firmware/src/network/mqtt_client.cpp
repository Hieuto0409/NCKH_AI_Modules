#include "network/mqtt_client.h"
#include "config/network_config.h"
#include <cstring>
namespace ppgfw {
MqttClient::MqttClient() = default;
void MqttClient::begin() {
#if APP_ENABLE_MQTT
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_OFF);
    configured_ = *config::network::kWifiSsid && *config::network::kMqttHost &&
                  *config::network::kAccessToken;
#endif
}
bool MqttClient::publish(const char* payload) {
#if APP_ENABLE_MQTT
    if (!configured_ || !payload || !*payload || std::strlen(payload) >= 1536 || count_ == queue_.size()) {
        ++rejected_;
        // A full queue still gets a bounded chance to drain after this measurement.
        if (configured_ && count_) armed_ = true;
        return false;
    }
    std::strcpy(queue_[(head_ + count_) % queue_.size()].data(), payload);
    ++count_;
    armed_ = true;
    return true;
#else
    (void)payload;
    return false;
#endif
}
#if APP_ENABLE_MQTT
void MqttClient::onEvent(void* context, esp_event_base_t, int32_t id, void* data) {
    auto* self = static_cast<MqttClient*>(context);
    if (id == MQTT_EVENT_CONNECTED) self->connected_.store(true);
    if (id == MQTT_EVENT_DISCONNECTED || id == MQTT_EVENT_ERROR) {
        self->connected_.store(false);
        self->failed_.store(true);
    }
    if (id == MQTT_EVENT_PUBLISHED)
        self->ack_id_.store(static_cast<esp_mqtt_event_handle_t>(data)->msg_id);
}
#endif
void MqttClient::stopRadio() {
#if APP_ENABLE_MQTT
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    if (radio_active_) {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
    }
#endif
    radio_active_ = false;
    connected_.store(false);
    failed_.store(false);
    ack_id_.store(-1);
    in_flight_ = -1;
}
void MqttClient::suspend() {
    // Preserve pending results, but no automatic retry on cancel/idle.
    stopRadio();
    armed_ = false;
}
bool MqttClient::connected() { return connected_.load(); }
void MqttClient::loop(uint32_t now_ms, bool upload_allowed) {
#if APP_ENABLE_MQTT
    if (!upload_allowed) { suspend(); return; }
    if (!configured_ || !armed_ || !count_) return;
    if (!radio_active_) {
        started_ms_ = now_ms;
        radio_active_ = true;
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.begin(config::network::kWifiSsid, config::network::kWifiPassword);
        return;
    }
    if (in_flight_ >= 0 && ack_id_.load() == in_flight_) {
        head_ = (head_ + 1) % queue_.size();
        --count_;
        ++acknowledged_;
        in_flight_ = -1;
        ack_id_.store(-1);
        if (!count_) { suspend(); return; }
    }
    // Unsigned elapsed time handles millis rollover. No unbounded reconnect loop.
    if (failed_.load() || now_ms - started_ms_ >= config::network::kUploadBudgetMs) {
        suspend(); return;
    }
    if (WiFi.status() != WL_CONNECTED) return;
    if (!client_) {
        esp_mqtt_client_config_t cfg{};
        cfg.host = config::network::kMqttHost;
        cfg.port = config::network::kMqttPort;
        cfg.username = config::network::kAccessToken;
        cfg.password = "";
        cfg.transport = MQTT_TRANSPORT_OVER_TCP;
        cfg.buffer_size = 2048;
        cfg.network_timeout_ms = 1000;
        cfg.disable_auto_reconnect = true;
        cfg.keepalive = 15;
        client_ = esp_mqtt_client_init(&cfg);
        if (!client_ || esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, onEvent, this) != ESP_OK ||
            esp_mqtt_client_start(client_) != ESP_OK) suspend();
        return;
    }
    if (connected() && in_flight_ < 0) {
        // QoS 1: release a queued result only after broker PUBACK, not socket write.
        in_flight_ = esp_mqtt_client_enqueue(client_, config::network::kMqttTopic,
                                            queue_[head_].data(), 0, 1, 0, true);
        if (in_flight_ < 0) suspend();
    }
#else
    (void)now_ms; (void)upload_allowed;
#endif
}
}
