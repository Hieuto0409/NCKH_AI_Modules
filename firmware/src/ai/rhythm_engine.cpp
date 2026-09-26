#include "ai/rhythm_engine.h"

#include "config/versions.h"
#include "acquisition/timestamp_service.h"
#include "ai/ecg_af_model_adapter.h"

#include <algorithm>

namespace ppgfw {

BranchResult RhythmEngine::infer(const FeaturePacket& packet) const {
    BranchResult result{};
    result.model_version = config::kEcgAfModelVersion;
    result.scaler_version = config::kEcgAfScalerVersion;
    result.required_modalities = ModalityEcg;
    if (packet.ecg_quality.status != QualityStatus::Good) {
        result.status = InferenceStatus::QualityRejected;
        result.reason_flags = packet.ecg_quality.reasons;
        return result;
    }
    if (packet.ecg_af.status == FeatureVectorStatus::NotReady) {
        result.status = InferenceStatus::NotReady;
        result.reason_flags = InferenceReasonInsufficientIntervals;
        return result;
    }
    if (packet.ecg_af.status == FeatureVectorStatus::QualityRejected) {
        result.status = InferenceStatus::QualityRejected;
        result.reason_flags = InferenceReasonIntervalQc;
        return result;
    }
    if (packet.ecg_af.status != FeatureVectorStatus::Ready) {
        result.status = InferenceStatus::InvalidInput;
        return result;
    }

    const uint64_t started = TimestampService::nowUs();
    const EcgAfModelResult inference = EcgAfModelAdapter::infer(packet.ecg_af.values);
    result.inference_latency_us = static_cast<uint32_t>(TimestampService::nowUs() - started);
    result.used_modalities = ModalityEcg;
    result.source = AlertSource::AiModel;
    if (!inference.model_available) {
        result.status = inference.error ? InferenceStatus::Error : InferenceStatus::NotReady;
        result.reason_flags = inference.error ? InferenceReasonModelError
                                              : InferenceReasonModelUnavailable;
        return result;
    }
    if (inference.error || !inference.valid) {
        result.status = InferenceStatus::Error;
        result.reason_flags = InferenceReasonModelError;
        return result;
    }
    result.status = InferenceStatus::Valid;
    result.score = inference.probability_af;
    result.confidence = std::max(inference.probability_af, inference.probability_non_af);
    result.alert = inference.probability_af > inference.probability_non_af;
    result.label = result.alert ? BranchLabel::Af : BranchLabel::NonAf;
    result.negative_conclusive = false;
    return result;
}

}
