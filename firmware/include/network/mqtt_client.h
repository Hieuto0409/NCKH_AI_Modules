#pragma once
#include "config/board_config.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#if APP_ENABLE_MQTT
#include <mqtt_client.h>
#include <WiFi.h>
#endif
namespace ppgfw {
class MqttClient {
public:
    MqttClient();
    void begin();
    // Only the application may permit radio operation, after acquisition stops.
    void loop(uint32_t now_ms, bool upload_allowed);
    void suspend();
    bool publish(const char* payload); // enqueue; does not enable the radio
    bool connected();
    size_t pending() const { return count_; }
    uint32_t acknowledged() const { return acknowledged_; }
    uint32_t rejected() const { return rejected_; }
    bool radioActive() const { return radio_active_; }
private:
    void stopRadio();
#if APP_ENABLE_MQTT
    static void onEvent(void* context, esp_event_base_t, int32_t id, void* data);
    esp_mqtt_client_handle_t client_{};
    std::array<std::array<char, 1536>, 4> queue_{};
#endif
    std::atomic<bool> connected_{false}, failed_{false};
    std::atomic<int> ack_id_{-1};
    size_t head_{}, count_{};
    uint32_t started_ms_{}, acknowledged_{}, rejected_{};
    int in_flight_{-1};
    bool configured_{}, armed_{}, radio_active_{};
};
}
