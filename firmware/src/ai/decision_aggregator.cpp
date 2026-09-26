#include "ai/decision_aggregator.h"

namespace ppgfw {

ResultSnapshot DecisionAggregator::aggregate(uint64_t timestamp_us, const FeaturePacket& packet,
                                              const BranchResult& stress,
                                              const BranchResult& low_o2,
                                              const BranchResult& rhythm) {
    ResultSnapshot result{};
    result.timestamp_us = timestamp_us;
    result.feature_packet = packet;
    result.stress = stress;
    result.low_o2 = low_o2;
    result.rhythm = rhythm;
    const bool all_valid_negative = stress.status == InferenceStatus::Valid &&
                                    low_o2.status == InferenceStatus::Valid &&
                                    rhythm.status == InferenceStatus::Valid &&
                                    !stress.alert && !low_o2.alert && !rhythm.alert;
    result.normal = all_valid_negative && stress.negative_conclusive &&
                    low_o2.negative_conclusive && rhythm.negative_conclusive;
    result.remeasure = packet.ppg_quality.status == QualityStatus::Poor ||
                       packet.ecg_quality.status == QualityStatus::Poor ||
                       stress.status == InferenceStatus::InvalidInput ||
                       low_o2.status == InferenceStatus::InvalidInput ||
                       rhythm.status == InferenceStatus::InvalidInput ||
                       stress.status == InferenceStatus::QualityRejected ||
                       low_o2.status == InferenceStatus::QualityRejected ||
                       rhythm.status == InferenceStatus::QualityRejected;
    return result;
}

}
