#pragma once
#include <cstdint>
#if __has_include("config/network_secrets.h")
#include "config/network_secrets.h"
#endif
#ifndef PPGFW_WIFI_SSID
#define PPGFW_WIFI_SSID ""
#endif
#ifndef PPGFW_WIFI_PASSWORD
#define PPGFW_WIFI_PASSWORD ""
#endif
#ifndef PPGFW_TB_HOST
#define PPGFW_TB_HOST ""
#endif
#ifndef PPGFW_TB_TOKEN
#define PPGFW_TB_TOKEN ""
#endif
namespace ppgfw::config::network {
inline constexpr const char* kWifiSsid = PPGFW_WIFI_SSID;
inline constexpr const char* kWifiPassword = PPGFW_WIFI_PASSWORD;
inline constexpr const char* kMqttHost = PPGFW_TB_HOST;
inline constexpr const char* kAccessToken = PPGFW_TB_TOKEN;
inline constexpr uint16_t kMqttPort = 1883;
inline constexpr const char* kMqttTopic = "v1/devices/me/telemetry";
inline constexpr uint32_t kUploadBudgetMs = 30000;
}
