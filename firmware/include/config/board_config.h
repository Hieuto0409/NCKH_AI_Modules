#pragma once

#ifndef APP_ENABLE_OLED
#define APP_ENABLE_OLED 1
#endif

#ifndef APP_ENABLE_MQTT
#define APP_ENABLE_MQTT 0
#endif

#ifndef APP_ENABLE_AI
#define APP_ENABLE_AI 1
#endif

#ifndef APP_ENABLE_BINARY_LOG
#define APP_ENABLE_BINARY_LOG 0
#endif

#ifndef APP_ENABLE_CSV_LOG
#define APP_ENABLE_CSV_LOG 0
#endif

namespace ppgfw::config {

inline constexpr unsigned long kSerialBaud = 921600;
inline constexpr bool kEnableOled = APP_ENABLE_OLED != 0;
inline constexpr bool kEnableMqtt = APP_ENABLE_MQTT != 0;
inline constexpr bool kEnableAi = APP_ENABLE_AI != 0;
inline constexpr bool kEnableBinaryLog = APP_ENABLE_BINARY_LOG != 0;
inline constexpr bool kEnableCsvLog = APP_ENABLE_CSV_LOG != 0;

}
