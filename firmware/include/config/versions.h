#pragma once

#include <cstdint>

namespace ppgfw::config {

inline constexpr const char* kFirmwareVersion = "0.4.0";
inline constexpr uint32_t kFirmwareVersionCode = 0x000400U;
inline constexpr uint16_t kRawSchemaVersion = 2;
inline constexpr uint16_t kFeatureSchemaVersion = 3;
inline constexpr uint16_t kPpgDspConfigVersion = 2;
inline constexpr uint16_t kEcgDspConfigVersion = 2;
inline constexpr uint16_t kQualityConfigVersion = 2;
inline constexpr uint16_t kSpo2AlgorithmVersion = 2;
inline constexpr uint16_t kSpo2CalibrationVersion = 0;
inline constexpr uint16_t kModelVersionUnavailable = 0;
inline constexpr uint16_t kScalerVersionUnavailable = 0;
inline constexpr uint16_t kStressModelVersion = 1;
inline constexpr uint16_t kStressScalerVersion = 1;
inline constexpr uint16_t kEcgAfModelVersion = 1;
inline constexpr uint16_t kEcgAfScalerVersion = 1;
inline constexpr const char* kAiRepositoryCommit =
    "7d4683581c8c01ff6878bcad31a601aff271f2a0";

}
