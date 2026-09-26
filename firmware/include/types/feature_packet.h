#pragma once

#include "types/metric_types.h"
#include "types/model_feature_types.h"
#include "types/quality_types.h"
#include "types/runtime_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ppgfw {

enum class FeatureSchema : uint16_t {
    V1 = 1,
    V2 = 2,
    V3 = 3
};

enum class PpgFeatureIndex : size_t {
    RedMean,
    RedStandardDeviation,
    RedMinimum,
    RedMaximum,
    IrMean,
    IrStandardDeviation,
    IrMinimum,
    IrMaximum,
    RedAc,
    RedDc,
    IrAc,
    IrDc,
    RedPerfusionIndex,
    IrPerfusionIndex,
    RedIrCorrelation,
    PulseRate,
    MeanPpi,
    SdnnPpi,
    RmssdPpi,
    Pnn50Ppi,
    PeakCount,
    MeanPulseAmplitude,
    PulseAmplitudeVariation,
    SignalRangeRed,
    SignalRangeIr,
    Spo2Ratio,
    SampleCompleteness,
    ClippingFraction,
    Count
};

inline constexpr size_t kPpgFeatureCount = static_cast<size_t>(PpgFeatureIndex::Count);

struct PpgFeatureSet {
    std::array<float, kPpgFeatureCount> values{};
    uint32_t validity_mask{};
};

struct EcgFeatureSet {
    float mean{};
    float standard_deviation{};
    float signal_range{};
    float heart_rate_bpm{};
    float mean_rr_ms{};
    float sdnn_ms{};
    float rmssd_ms{};
    float pnn50_percent{};
    float mean_rpeak_amplitude{};
    uint16_t rpeak_count{};
    uint16_t valid_feature_count{};
};

struct FusionFeatureSet {
    float heart_rate_difference_bpm{};
    float pulse_arrival_candidate_ms{};
    uint16_t matched_beat_count{};
    bool pulse_arrival_valid{};
};

struct FeaturePacket {
    FeatureSchema schema{FeatureSchema::V3};
    uint64_t window_start_us{};
    uint64_t window_end_us{};
    uint32_t validity_mask{};
    WindowMetadata ppg_window{};
    WindowMetadata ecg_window{};
    WindowMetadata spo2_window{};
    QualitySummary ppg_quality{};
    QualitySummary ecg_quality{};
    MetricSnapshot metrics{};
    PpgFeatureSet ppg{};
    EcgFeatureSet ecg{};
    FusionFeatureSet fusion{};
    StressPpgFeatureVector stress_ppg{};
    EcgAfFeatureVector ecg_af{};
};

}
