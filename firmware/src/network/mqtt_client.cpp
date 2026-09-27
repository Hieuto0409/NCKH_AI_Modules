#include "network/mqtt_client.h"
#include "config/network_config.h"
#include <cstring>
#if APP_ENABLE_MQTT && APP_TB_HTTPS
#include "config/thingsboard_ca.h"
#include <Arduino.h>
#include <ctime>
#include <cstdio>
#include <lwip/apps/sntp.h>
#endif
namespace ppgfw {
MqttClient::MqttClient() = default;
void MqttClient::begin() {
#if APP_ENABLE_MQTT
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_OFF);
    configured_ = *config::network::kWifiSsid && *config::network::kMqttHost &&
                  *config::network::kAccessToken;
    stage_ = configured_ ? UploadStage::Ready : UploadStage::NotConfigured;
#endif
}
bool MqttClient::publish(const char* payload) {
#if APP_ENABLE_MQTT
    if (!configured_ || !payload || !*payload || std::strlen(payload) >= 1536 || count_ == queue_.size()) {
        ++rejected_;
        latest_rejected_ = configured_;
        // A full queue still gets a bounded chance to drain after this measurement.
        if (configured_ && count_) armed_ = true;
        return false;
    }
    std::strcpy(queue_[(head_ + count_) % queue_.size()].data(), payload);
    ++count_;
    armed_ = true;
    latest_rejected_ = false;
    stage_ = UploadStage::Queued;
    return true;
#else
    (void)payload;
    return false;
#endif
}
bool MqttClient::retryPending() {
    if (!configured_ || !count_ || radio_active_) return false;
    armed_ = true;
    stage_ = UploadStage::Queued;
    return true;
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
#if APP_TB_HTTPS
    if (http_) { esp_http_client_cleanup(http_); http_ = nullptr; }
    if (clock_started_) sntp_stop();
    clock_started_ = false;
#endif
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
    if (configured_) stage_ = count_ ? UploadStage::Deferred :
        (acknowledged_ ? UploadStage::Sent : UploadStage::Ready);
}
bool MqttClient::connected() { return connected_.load(); }
void MqttClient::loop(uint32_t now_ms, bool upload_allowed) {
#if APP_ENABLE_MQTT
    if (!upload_allowed) { suspend(); return; }
    if (!configured_ || !armed_ || !count_) return;
    if (!radio_active_) {
        started_ms_ = now_ms;
        radio_active_ = true;
        stage_ = UploadStage::Wifi;
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
#if APP_TB_HTTPS
    loopHttps();
#else
    stage_ = UploadStage::Sending;
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
#endif
#else
    (void)now_ms; (void)upload_allowed;
#endif
}
#if APP_ENABLE_MQTT && APP_TB_HTTPS
void MqttClient::loopHttps() {
    // Obtain wall time for certificate validation only after sensing has stopped.
    if (time(nullptr) < 1704067200) {
        stage_ = UploadStage::Clock;
        if (!clock_started_) {
            configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
            clock_started_ = true;
        }
        return;
    }
    stage_ = UploadStage::Sending;
    if (!http_) {
        const int n = std::snprintf(url_.data(), url_.size(), "https://%s/api/v1/%s/telemetry",
            config::network::kMqttHost, config::network::kAccessToken);
        if (n < 0 || size_t(n) >= url_.size()) { suspend(); return; }
        esp_http_client_config_t cfg{};
        cfg.url = url_.data();
        cfg.cert_pem = config::network::kThingsBoardRootCa;
        cfg.method = HTTP_METHOD_POST;
        cfg.timeout_ms = 1000;
        cfg.is_async = true;
        cfg.disable_auto_redirect = true; // never send a token to another destination
        cfg.buffer_size = 1024;
        cfg.buffer_size_tx = 2048;
        cfg.user_agent = "EdgeAI-PPG/0.5";
        http_ = esp_http_client_init(&cfg);
        if (!http_) { suspend(); return; }
        if (esp_http_client_set_header(http_, "Content-Type", "application/json") != ESP_OK ||
            esp_http_client_set_post_field(http_, queue_[head_].data(), std::strlen(queue_[head_].data())) != ESP_OK) {
            suspend(); return;
        }
    }
    const auto err = esp_http_client_perform(http_);
    if (err == ESP_ERR_HTTP_EAGAIN) return;
    const int status = esp_http_client_get_status_code(http_);
    esp_http_client_cleanup(http_); http_ = nullptr;
    if (err != ESP_OK || status != 200) { suspend(); return; }
    // ThingsBoard HTTP 200 is the application acknowledgement. No socket-write ACK.
    head_ = (head_ + 1) % queue_.size();
    --count_; ++acknowledged_;
    if (!count_) suspend();
}
#endif
}
