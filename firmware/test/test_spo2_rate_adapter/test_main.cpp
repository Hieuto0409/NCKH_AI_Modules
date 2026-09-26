#include <unity.h>

#include "config/sampling.h"
#include "metrics/spo2_rate_adapter.h"

#include <cmath>
#include <cstdio>

using namespace ppgfw;

namespace {

PpgSample makeSample(uint32_t index, uint16_t flags = SampleValid | SampleContact) {
    const double phase = static_cast<double>(index) * 2.0 * 3.141592653589793 / 40.0;
    return {static_cast<uint64_t>(index) * config::kPpgSamplePeriodUs,
            index,
            static_cast<uint32_t>(40000.0 + 500.0 * std::sin(phase)),
            static_cast<uint32_t>(60000.0 + 1200.0 * std::sin(phase)),
            flags};
}

Spo2InputConfig inputConfig() {
    return {config::kPpgSampleRateHz, config::kMax30102RedLedCurrent,
            config::kMax30102IrLedCurrent, config::kMax30102AdcRangeNa};
}

}

void test_800_samples_become_400_pairs_and_four_seconds_ready() {
    Spo2RateAdapter200To100 adapter{};
    adapter.reset(inputConfig());
    for (uint32_t index = 0; index < 799; ++index) {
        adapter.push(makeSample(index));
    }
    TEST_ASSERT_EQUAL_UINT32(399, adapter.modulePairCount());
    TEST_ASSERT_EQUAL(static_cast<int>(Spo2EstimatorStatus::NeedData),
                      static_cast<int>(adapter.evaluate(true).estimator_status));
    adapter.push(makeSample(799));
    const Spo2Result result = adapter.evaluate(true);
    TEST_ASSERT_EQUAL_UINT32(800, result.source_sample_count);
    TEST_ASSERT_EQUAL_UINT32(400, result.module_pair_count);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(Spo2EstimatorStatus::NeedData),
                          static_cast<int>(result.estimator_status));
}

void test_quality_flag_rejects_complete_window() {
    Spo2RateAdapter200To100 adapter{};
    adapter.reset(inputConfig());
    for (uint32_t index = 0; index < 800; ++index) {
        adapter.push(makeSample(index));
    }
    const Spo2Result result = adapter.evaluate(false);
    TEST_ASSERT_EQUAL(static_cast<int>(Spo2EstimatorStatus::QualityRejected),
                      static_cast<int>(result.estimator_status));
    TEST_ASSERT_EQUAL(static_cast<int>(ValueStatus::InvalidSignal),
                      static_cast<int>(result.status));
}

void test_gap_and_overflow_reset_stream() {
    Spo2RateAdapter200To100 adapter{};
    adapter.reset(inputConfig());
    adapter.push(makeSample(0));
    adapter.push(makeSample(1));
    TEST_ASSERT_EQUAL_UINT32(1, adapter.modulePairCount());
    adapter.push(makeSample(3));
    TEST_ASSERT_EQUAL_UINT32(1, adapter.resetCount());
    TEST_ASSERT_EQUAL_UINT32(0, adapter.modulePairCount());
    adapter.push(makeSample(4));
    TEST_ASSERT_EQUAL_UINT32(1, adapter.modulePairCount());
    adapter.push(makeSample(5, SampleValid | SampleContact | SampleOverflowContext));
    TEST_ASSERT_EQUAL_UINT32(2, adapter.resetCount());
    TEST_ASSERT_EQUAL_UINT32(0, adapter.modulePairCount());
}

void test_sensor_configuration_change_resets_stream() {
    Spo2RateAdapter200To100 adapter{};
    Spo2InputConfig config_value = inputConfig();
    adapter.reset(config_value);
    adapter.push(makeSample(0));
    adapter.push(makeSample(1));
    ++config_value.red_led_current;
    adapter.updateConfig(config_value);
    TEST_ASSERT_EQUAL_UINT32(1, adapter.resetCount());
    TEST_ASSERT_EQUAL_UINT32(0, adapter.sourceSampleCount());
    TEST_ASSERT_EQUAL_UINT32(0, adapter.modulePairCount());
}

int main(int, char**) {
    UNITY_BEGIN();
    std::printf("*** OFFLINE TEST \xE2\x80\x94 NOT A SENSOR MEASUREMENT ***\n");
    RUN_TEST(test_800_samples_become_400_pairs_and_four_seconds_ready);
    RUN_TEST(test_quality_flag_rejects_complete_window);
    RUN_TEST(test_gap_and_overflow_reset_stream);
    RUN_TEST(test_sensor_configuration_change_resets_stream);
    return UNITY_END();
}
