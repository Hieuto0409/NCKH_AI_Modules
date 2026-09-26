#pragma once

#include <cstdint>

namespace ppgfw::config::pins {

inline constexpr int8_t kPpgSda = 12;
inline constexpr int8_t kPpgScl = 13;
inline constexpr int8_t kOledCs = 42;
inline constexpr int8_t kOledDc = 41;
inline constexpr int8_t kOledReset = 40;
inline constexpr int8_t kOledMosi = 39;
inline constexpr int8_t kOledSclk = 38;
inline constexpr int8_t kButton1 = 48;
inline constexpr int8_t kButton2 = 47;
inline constexpr int8_t kBatteryAdc = 8;
inline constexpr int8_t kEcgOut = 1;
inline constexpr int8_t kEcgLeadOffPositive = 5;
inline constexpr int8_t kEcgLeadOffNegative = 6;

}

