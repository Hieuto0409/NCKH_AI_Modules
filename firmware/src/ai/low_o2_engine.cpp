#include "ai/low_o2_engine.h"

#include "config/thresholds.h"
#include "config/versions.h"

namespace ppgfw {

BranchResult LowO2Engine::infer(const FeaturePacket& packet) const {
    BranchResult result{};
    result.model_version = config::kModelVersionUnavailable;
    result.scaler_version = config::kScalerVersionUnavailable;
    result.required_modalities = ModalityPpg;
    if (packet.ppg_quality.status != QualityStatus::Good) {
        result.status = InferenceStatus::QualityRejected;
        result.reason_flags = packet.ppg_quality.reasons;
        return result;
    }
    if (packet.metrics.spo2.status == ValueStatus::NotAvailable) {
        result.status = InferenceStatus::NotReady;
        result.reason_flags = InferenceReasonInsufficientWindow;
        return result;
    }
    if (packet.metrics.spo2.status == ValueStatus::InvalidSignal) {
        result.status = InferenceStatus::QualityRejected;
        return result;
    }
    if (packet.metrics.spo2.status == ValueStatus::Error) {
        result.status = InferenceStatus::Error;
        return result;
    }
    if (config::candidate::kLowO2RuleValidated &&
        packet.metrics.spo2.status == ValueStatus::Valid) {
        result.status = InferenceStatus::Valid;
        result.score = packet.metrics.spo2.percent;
        result.confidence = packet.ppg_quality.score;
        result.alert = packet.metrics.spo2.percent < config::candidate::kLowO2AlertPercent;
        result.source = AlertSource::AlgorithmicRule;
        result.used_modalities = ModalityPpg;
        result.label = result.alert ? BranchLabel::LowO2 : BranchLabel::NotLowO2;
        result.negative_conclusive = !result.alert;
        return result;
    }
    result.status = InferenceStatus::NotReady;
    result.reason_flags = InferenceReasonRuleUnvalidated;
    result.used_modalities = ModalityPpg;
    return result;
}

}
