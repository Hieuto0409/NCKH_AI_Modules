#pragma once

#include <cstdint>

namespace ppgfw::config::network {

inline constexpr const char* kWifiSsid = "";
inline constexpr const char* kWifiPassword = "";
inline constexpr const char* kMqttHost = "";
inline constexpr uint16_t kMqttPort = 1883;
inline constexpr const char* kMqttTopic = "ppgfw/telemetry";
inline constexpr uint32_t kReconnectBackoffMs = 5000;

}

