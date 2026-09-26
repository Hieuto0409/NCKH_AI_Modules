#include "ai/stress_engine.h"

#include "config/versions.h"
#include "acquisition/timestamp_service.h"

#include <stress_ppg_model.h>

namespace ppgfw {

BranchResult StressEngine::infer(const FeaturePacket& packet) const {
    BranchResult result{};
    result.model_version = config::kStressModelVersion;
    result.scaler_version = config::kStressScalerVersion;
    result.required_modalities = ModalityPpg;
    if (packet.ppg_quality.status != QualityStatus::Good) {
        result.status = InferenceStatus::QualityRejected;
        result.reason_flags = packet.ppg_quality.reasons;
        return result;
    }
    if (packet.stress_ppg.status == FeatureVectorStatus::NotReady) {
        result.status = InferenceStatus::NotReady;
        result.reason_flags = InferenceReasonInsufficientWindow;
        return result;
    }
    if (packet.stress_ppg.status == FeatureVectorStatus::QualityRejected) {
        result.status = InferenceStatus::QualityRejected;
        result.reason_flags = InferenceReasonIntervalQc;
        return result;
    }
    if (packet.stress_ppg.status != FeatureVectorStatus::Ready) {
        result.status = InferenceStatus::InvalidInput;
        return result;
    }

    const uint64_t started = TimestampService::nowUs();
    const stress_ppg::Result inference =
        stress_ppg::infer(packet.stress_ppg.values.data(), packet.stress_ppg.values.size());
    result.inference_latency_us = static_cast<uint32_t>(TimestampService::nowUs() - started);
    result.used_modalities = ModalityPpg;
    result.source = AlertSource::AiModel;
    if (!inference.valid) {
        result.status = InferenceStatus::Error;
        result.reason_flags = InferenceReasonModelError;
        return result;
    }
    result.status = InferenceStatus::Valid;
    result.score = static_cast<float>(inference.stress_probability);
    result.confidence = static_cast<float>(inference.stress
                                               ? inference.stress_probability
                                               : 1.0 - inference.stress_probability);
    result.alert = inference.stress;
    result.label = inference.stress ? BranchLabel::Stress : BranchLabel::Baseline;
    result.negative_conclusive = !inference.stress;
    return result;
}

}
