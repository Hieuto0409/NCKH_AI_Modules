#include <unity.h>

#include "ai/decision_aggregator.h"
#include "types/feature_packet.h"

using namespace ppgfw;

void test_ppg_feature_schema_has_28_fields() {
    TEST_ASSERT_EQUAL_UINT32(28, kPpgFeatureCount);
    TEST_ASSERT_EQUAL_UINT16(2, static_cast<uint16_t>(FeatureSchema::V2));
}

void test_normal_requires_all_three_valid_negative_branches() {
    FeaturePacket packet{};
    packet.ppg_quality.status = QualityStatus::Good;
    packet.ecg_quality.status = QualityStatus::Good;
    BranchResult negative{};
    negative.status = InferenceStatus::Valid;
    negative.negative_conclusive = true;
    const auto normal = DecisionAggregator::aggregate(1, packet, negative, negative, negative);
    TEST_ASSERT_TRUE(normal.normal);
    BranchResult unavailable{};
    unavailable.status = InferenceStatus::NotReady;
    const auto incomplete = DecisionAggregator::aggregate(1, packet, unavailable, negative, negative);
    TEST_ASSERT_FALSE(incomplete.normal);
}

void test_non_af_does_not_imply_normal_rhythm() {
    FeaturePacket packet{};
    packet.ppg_quality.status = QualityStatus::Good;
    packet.ecg_quality.status = QualityStatus::Good;
    BranchResult negative{};
    negative.status = InferenceStatus::Valid;
    negative.negative_conclusive = true;
    BranchResult non_af = negative;
    non_af.label = BranchLabel::NonAf;
    non_af.negative_conclusive = false;
    const auto aggregate = DecisionAggregator::aggregate(1, packet, negative, negative, non_af);
    TEST_ASSERT_FALSE(aggregate.normal);
    TEST_ASSERT_FALSE(aggregate.remeasure);
}

void test_poor_signal_requires_remeasurement() {
    FeaturePacket packet{};
    packet.ppg_quality.status = QualityStatus::Poor;
    packet.ecg_quality.status = QualityStatus::Good;
    BranchResult result{};
    result.status = InferenceStatus::InvalidInput;
    const auto aggregate = DecisionAggregator::aggregate(1, packet, result, result, result);
    TEST_ASSERT_TRUE(aggregate.remeasure);
}

void test_branch_contract_carries_versions_and_modalities() {
    BranchResult branch{};
    branch.model_version = 7;
    branch.scaler_version = 3;
    branch.required_modalities = ModalityPpg | ModalityEcg;
    TEST_ASSERT_EQUAL_UINT16(7, branch.model_version);
    TEST_ASSERT_EQUAL_UINT16(3, branch.scaler_version);
    TEST_ASSERT_BITS_HIGH(ModalityPpg | ModalityEcg, branch.required_modalities);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ppg_feature_schema_has_28_fields);
    RUN_TEST(test_normal_requires_all_three_valid_negative_branches);
    RUN_TEST(test_poor_signal_requires_remeasurement);
    RUN_TEST(test_branch_contract_carries_versions_and_modalities);
    RUN_TEST(test_non_af_does_not_imply_normal_rhythm);
    return UNITY_END();
}
