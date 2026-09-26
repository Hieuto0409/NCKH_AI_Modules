#pragma once

#include <cstddef>
#include <cstdint>

namespace ppgfw::config::model {

inline constexpr size_t kStressFeatureCount = 14;
inline constexpr uint64_t kStressWindowUs = 60000000ULL;
inline constexpr uint16_t kStressMinimumBeatCount = 40;
inline constexpr double kStressMinimumPpiMs = 60000.0 / 180.0;
inline constexpr double kStressMaximumPpiMs = 60000.0 / 40.0;
inline constexpr double kStressMedianTolerance = 0.30;
inline constexpr double kStressMinimumValidPpiRatio = 0.65;

inline constexpr size_t kEcgAfFeatureCount = 9;
inline constexpr double kEcgAfMinimumRrMs = 200.0;
inline constexpr double kEcgAfMaximumRrMs = 2000.0;
inline constexpr size_t kEcgAfMinimumRrCount = 3;

inline constexpr uint32_t kSpo2InputRateHz = 200;
inline constexpr uint32_t kSpo2ModuleRateHz = 100;
inline constexpr uint32_t kSpo2RequiredPairs = 400;

}
