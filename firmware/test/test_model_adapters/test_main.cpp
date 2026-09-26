#include <unity.h>

#include "ai/rhythm_engine.h"
#include "ai/stress_engine.h"
#include "features/ecg_af_feature_adapter.h"
#include "features/stress_ppg_60s_adapter.h"
#include "types/model_feature_types.h"

#include <stress_ppg_model.h>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace ppgfw;

void test_stress_feature_order_matches_pinned_model() {
    const char* expected[] = {
        "mean_hr_bpm", "std_hr_bpm", "min_hr_bpm", "max_hr_bpm",
        "mean_pp_ms", "median_pp_ms", "sdnn_ms", "rmssd_ms",
        "sdsd_ms", "pnn20_pct", "pnn50_pct", "cvnn",
        "beat_count", "valid_rr_ratio"};
    TEST_ASSERT_EQUAL_UINT32(config::model::kStressFeatureCount,
                             stress_ppg::kFeatureCount);
    for (size_t index = 0; index < stress_ppg::kFeatureCount; ++index) {
        TEST_ASSERT_EQUAL_STRING(expected[index], stress_ppg::kFeatureNames[index]);
    }
}

void test_stress_adapter_builds_exact_constant_interval_units() {
    StressPpg60sAdapter adapter{};
    adapter.reset(0);
    for (uint64_t second = 0; second <= 60; ++second) {
        adapter.addPeak(second * 1000000ULL);
    }
    const auto output = adapter.build(60000000ULL);
    TEST_ASSERT_EQUAL(static_cast<int>(FeatureVectorStatus::Ready),
                      static_cast<int>(output.status));
    TEST_ASSERT_EQUAL_UINT16(60, output.beat_count);
    TEST_ASSERT_EQUAL_UINT16(59, output.raw_interval_count);
    TEST_ASSERT_EQUAL_UINT16(59, output.clean_interval_count);
    TEST_ASSERT_TRUE(std::fabs(
        output.values[static_cast<size_t>(StressPpgFeatureIndex::MeanHrBpm)] -
        60.0) < 1e-12);
    TEST_ASSERT_TRUE(std::fabs(
        output.values[static_cast<size_t>(StressPpgFeatureIndex::MeanPpMs)] -
        1000.0) < 1e-12);
    TEST_ASSERT_TRUE(std::fabs(
        output.values[static_cast<size_t>(StressPpgFeatureIndex::SdnnMs)]) <
        1e-12);
    TEST_ASSERT_TRUE(std::fabs(
        output.values[static_cast<size_t>(StressPpgFeatureIndex::ValidRrRatio)] -
        1.0) < 1e-12);
}

void test_stress_model_matches_pinned_python_fixture() {
    FeaturePacket packet{};
    packet.ppg_quality.status = QualityStatus::Good;
    packet.stress_ppg.status = FeatureVectorStatus::Ready;
    packet.stress_ppg.values = {
        90.09228867656934, 9.035973070988689, 72.45283018867924,
        123.87096774193549, 671.875, 671.875, 60.48228723909283,
        87.13422523306909, 87.85392657484061, 68.85245901639344,
        40.98360655737705, 0.09002014844888234, 92.0,
        0.6813186813186813};
    const BranchResult result = StressEngine{}.infer(packet);
    TEST_ASSERT_EQUAL(static_cast<int>(InferenceStatus::Valid),
                      static_cast<int>(result.status));
    TEST_ASSERT_EQUAL(static_cast<int>(BranchLabel::Stress),
                      static_cast<int>(result.label));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.648799505354633710F, result.score);
}

void test_ecg_af_adapter_uses_seconds_percent_and_exact_order() {
    const float rr_ms[] = {800.0F, 1000.0F, 900.0F, 1100.0F};
    const auto output = EcgAfFeatureAdapter::build(rr_ms, 4);
    TEST_ASSERT_EQUAL(static_cast<int>(FeatureVectorStatus::Ready),
                      static_cast<int>(output.status));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.95F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::MeanRr)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.95F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::MedianRr)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.129099445F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::Sdnn)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.173205081F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::Rmssd)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 100.0F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::Pnn50)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.15F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::IqrRr)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.8F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::MinRr)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 1.1F,
        output.values[static_cast<size_t>(EcgAfFeatureIndex::MaxRr)]);
}

void test_model_wrappers_preserve_not_ready_and_quality_rejected() {
    FeaturePacket packet{};
    packet.ppg_quality.status = QualityStatus::Good;
    packet.ecg_quality.status = QualityStatus::Good;

    BranchResult stress = StressEngine{}.infer(packet);
    TEST_ASSERT_EQUAL(static_cast<int>(InferenceStatus::NotReady),
                      static_cast<int>(stress.status));
    TEST_ASSERT_BITS_HIGH(InferenceReasonInsufficientWindow, stress.reason_flags);

    BranchResult rhythm = RhythmEngine{}.infer(packet);
    TEST_ASSERT_EQUAL(static_cast<int>(InferenceStatus::NotReady),
                      static_cast<int>(rhythm.status));
    TEST_ASSERT_BITS_HIGH(InferenceReasonInsufficientIntervals, rhythm.reason_flags);

    packet.ppg_quality.status = QualityStatus::Poor;
    packet.ecg_quality.status = QualityStatus::Poor;
    stress = StressEngine{}.infer(packet);
    rhythm = RhythmEngine{}.infer(packet);
    TEST_ASSERT_EQUAL(static_cast<int>(InferenceStatus::QualityRejected),
                      static_cast<int>(stress.status));
    TEST_ASSERT_EQUAL(static_cast<int>(InferenceStatus::QualityRejected),
                      static_cast<int>(rhythm.status));
}

int main(int, char**) {
    UNITY_BEGIN();
    std::printf("*** OFFLINE TEST \xE2\x80\x94 NOT A SENSOR MEASUREMENT ***\n");
    RUN_TEST(test_stress_feature_order_matches_pinned_model);
    RUN_TEST(test_stress_adapter_builds_exact_constant_interval_units);
    RUN_TEST(test_stress_model_matches_pinned_python_fixture);
    RUN_TEST(test_ecg_af_adapter_uses_seconds_percent_and_exact_order);
    RUN_TEST(test_model_wrappers_preserve_not_ready_and_quality_rejected);
    return UNITY_END();
}
