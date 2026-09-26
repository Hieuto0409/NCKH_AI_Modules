#pragma once

#include "types/quality_types.h"

#include <cstdint>
#include <limits>

namespace ppgfw {

inline constexpr float kUnavailableValue = std::numeric_limits<float>::quiet_NaN();

struct MetricValue {
    ValueStatus status{ValueStatus::NotAvailable};
    float value{kUnavailableValue};
};

struct HrvMetrics {
    ValueStatus status{ValueStatus::NotAvailable};
    float mean_interval_ms{kUnavailableValue};
    float sdnn_ms{kUnavailableValue};
    float rmssd_ms{kUnavailableValue};
    float pnn50_percent{kUnavailableValue};
    uint16_t interval_count{};
};

enum class Spo2EstimatorStatus : uint8_t {
    NeedData,
    Ok,
    InvalidInput,
    ContactLost,
    Clipped,
    QualityRejected,
    AlgorithmRejected
};

struct Spo2Result {
    ValueStatus status{ValueStatus::NotAvailable};
    float percent{kUnavailableValue};
    float ratio{kUnavailableValue};
    float red_ac{kUnavailableValue};
    float red_dc{kUnavailableValue};
    float ir_ac{kUnavailableValue};
    float ir_dc{kUnavailableValue};
    uint16_t algorithm_version{};
    uint16_t calibration_version{};
    Spo2EstimatorStatus estimator_status{Spo2EstimatorStatus::NeedData};
    uint32_t source_sample_count{};
    uint32_t module_pair_count{};
    uint32_t adapter_reset_count{};
    bool clinically_validated{};
};

struct MetricSnapshot {
    MetricValue ecg_heart_rate_bpm{};
    MetricValue ppg_pulse_rate_bpm{};
    HrvMetrics hrv{};
    HrvMetrics prv{};
    Spo2Result spo2{};
};

struct SignalStatistics {
    uint32_t count{};
    double mean{};
    double variance{};
    double minimum{};
    double maximum{};
};

}
