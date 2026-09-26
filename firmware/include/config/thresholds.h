#pragma once

#include <cstdint>

namespace ppgfw::config::candidate {

inline constexpr uint32_t kFingerIrMinimum = 50000;
inline constexpr uint32_t kPpgRawClipLow = 1000;
inline constexpr uint32_t kPpgRawClipHigh = 260000;
inline constexpr int16_t kEcgRawClipLow = 8;
inline constexpr int16_t kEcgRawClipHigh = 4087;
inline constexpr float kMinimumCompleteness = 0.98F;
inline constexpr float kMaximumClipFraction = 0.01F;
inline constexpr float kMinimumPpgPerfusionIndex = 0.001F;
inline constexpr float kMinimumRedIrCorrelation = 0.50F;
inline constexpr float kMinimumEcgStandardDeviation = 4.0F;
inline constexpr uint32_t kPpgPeakRefractoryMs = 280;
inline constexpr uint32_t kEcgPeakRefractoryMs = 240;
inline constexpr float kPpgPeakThresholdScale = 0.45F;
inline constexpr float kEcgPeakThresholdScale = 0.55F;
inline constexpr float kMinimumBeatIntervalMs = 250.0F;
inline constexpr float kMaximumBeatIntervalMs = 2000.0F;
inline constexpr float kPpgDcAlpha = 0.0025F;
inline constexpr float kPpgLowPassAlpha = 0.18F;
inline constexpr float kEcgDcAlpha = 0.004F;
inline constexpr float kEcgLowPassAlpha = 0.24F;
inline constexpr float kSpo2RatioMinimum = 0.2F;
inline constexpr float kSpo2RatioMaximum = 1.8F;
inline constexpr float kSpo2PolynomialA = -25.0F;
inline constexpr float kSpo2PolynomialB = 110.0F;
inline constexpr float kLowO2AlertPercent = 92.0F;
inline constexpr bool kSpo2CalibrationValidated = false;
inline constexpr bool kLowO2RuleValidated = false;

}
