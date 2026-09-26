#pragma once

#include "config/model_contracts.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ppgfw {

enum class FeatureVectorStatus : uint8_t {
    NotReady,
    Ready,
    QualityRejected,
    InvalidInput
};

enum class StressPpgFeatureIndex : size_t {
    MeanHrBpm,
    StdHrBpm,
    MinHrBpm,
    MaxHrBpm,
    MeanPpMs,
    MedianPpMs,
    SdnnMs,
    RmssdMs,
    SdsdMs,
    Pnn20Pct,
    Pnn50Pct,
    Cvnn,
    BeatCount,
    ValidRrRatio,
    Count
};

enum class EcgAfFeatureIndex : size_t {
    MeanRr,
    MedianRr,
    Sdnn,
    Rmssd,
    Pnn50,
    CvRr,
    IqrRr,
    MinRr,
    MaxRr,
    Count
};

static_assert(static_cast<size_t>(StressPpgFeatureIndex::Count) ==
              config::model::kStressFeatureCount);
static_assert(static_cast<size_t>(EcgAfFeatureIndex::Count) ==
              config::model::kEcgAfFeatureCount);

struct StressPpgFeatureVector {
    FeatureVectorStatus status{FeatureVectorStatus::NotReady};
    std::array<double, config::model::kStressFeatureCount> values{};
    uint64_t window_start_us{};
    uint64_t window_end_us{};
    uint16_t beat_count{};
    uint16_t raw_interval_count{};
    uint16_t clean_interval_count{};
};

struct EcgAfFeatureVector {
    FeatureVectorStatus status{FeatureVectorStatus::NotReady};
    std::array<float, config::model::kEcgAfFeatureCount> values{};
    uint16_t interval_count{};
};

}
