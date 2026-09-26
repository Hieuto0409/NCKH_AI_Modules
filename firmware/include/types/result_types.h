#pragma once

#include "types/feature_packet.h"

#include <cstdint>

namespace ppgfw {

enum class InferenceStatus : uint8_t {
    Valid,
    InvalidInput,
    NotReady,
    QualityRejected,
    Error
};

enum class BranchLabel : uint8_t {
    None,
    Baseline,
    Stress,
    Af,
    NonAf,
    LowO2,
    NotLowO2
};

enum InferenceReason : uint32_t {
    InferenceReasonNone = 0,
    InferenceReasonInsufficientWindow = 1U << 16U,
    InferenceReasonInsufficientIntervals = 1U << 17U,
    InferenceReasonIntervalQc = 1U << 18U,
    InferenceReasonModelUnavailable = 1U << 19U,
    InferenceReasonRuleUnvalidated = 1U << 20U,
    InferenceReasonModelError = 1U << 21U
};

enum class AlertSource : uint8_t {
    None,
    AlgorithmicRule,
    AiModel
};

enum ModalityMask : uint8_t {
    ModalityNone = 0,
    ModalityPpg = 1U << 0U,
    ModalityEcg = 1U << 1U
};

struct BranchResult {
    InferenceStatus status{InferenceStatus::NotReady};
    float score{};
    float confidence{};
    bool alert{};
    uint32_t reason_flags{};
    uint16_t model_version{};
    uint16_t scaler_version{};
    uint8_t required_modalities{ModalityNone};
    uint8_t used_modalities{ModalityNone};
    AlertSource source{AlertSource::None};
    BranchLabel label{BranchLabel::None};
    uint32_t inference_latency_us{};
    bool negative_conclusive{};
};

struct ResultSnapshot {
    uint64_t timestamp_us{};
    FeaturePacket feature_packet{};
    BranchResult stress{};
    BranchResult low_o2{};
    BranchResult rhythm{};
    bool normal{};
    bool remeasure{};
};

}
