#pragma once

#include <cstddef>
#include <cstdint>

namespace ppgfw::config {

inline constexpr uint32_t kPpgSampleRateHz = 200;
inline constexpr uint32_t kEcgSampleRateHz = 500;
inline constexpr uint64_t kPpgSamplePeriodUs = 1000000ULL / kPpgSampleRateHz;
inline constexpr uint64_t kEcgSamplePeriodUs = 1000000ULL / kEcgSampleRateHz;
inline constexpr uint8_t kMax30102SampleAverage = 1;
inline constexpr uint16_t kMax30102PulseWidthUs = 411;
inline constexpr uint16_t kMax30102AdcRangeNa = 8192;
inline constexpr uint8_t kMax30102RedLedCurrent = 0x3F;
inline constexpr uint8_t kMax30102IrLedCurrent = 0x3F;
inline constexpr uint32_t kI2cClockHz = 400000;
inline constexpr size_t kPpgQueueCapacity = 512;
inline constexpr size_t kEcgQueueCapacity = 1024;
inline constexpr size_t kLoggerQueueCapacity = 512;
inline constexpr size_t kIntervalCapacity = 256;
inline constexpr uint32_t kWarmupMs = 3000;
inline constexpr uint32_t kContactStableMs = 500;
inline constexpr uint32_t kMeasurementWindowMs = 60000;
inline constexpr uint32_t kResultHoldMs = 10000;
inline constexpr uint32_t kOledRefreshMs = 250;
inline constexpr uint32_t kTelemetryPeriodMs = 1000;

}
