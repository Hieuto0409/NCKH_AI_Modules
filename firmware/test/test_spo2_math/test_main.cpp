#include <unity.h>

#include "dsp/ac_dc_estimator.h"
#include "metrics/heart_rate.h"
#include "metrics/hrv.h"
#include "metrics/spo2.h"

using namespace ppgfw;

void test_ac_dc_estimator_fixed_vector() {
    AcDcEstimator estimator;
    estimator.reset();
    estimator.add(100, 200);
    estimator.add(110, 220);
    estimator.add(90, 180);
    const auto signal = estimator.snapshot();
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 100.0F, signal.red_dc);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 200.0F, signal.ir_dc);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.0F, signal.correlation);
}

void test_spo2_ratio_is_available_but_percent_is_unvalidated() {
    AcDcSnapshot signal{};
    signal.red_ac = 1000.0F;
    signal.red_dc = 100000.0F;
    signal.ir_ac = 2000.0F;
    signal.ir_dc = 100000.0F;
    QualitySummary quality{};
    quality.status = QualityStatus::Good;
    const auto result = RatioOfRatiosSpo2{}.compute(signal, quality);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.5F, result.ratio);
    TEST_ASSERT_EQUAL(static_cast<int>(ValueStatus::NotAvailable),
                      static_cast<int>(result.status));
}

void test_hr_and_hrv_use_intervals() {
    const float intervals[] = {1000.0F, 1020.0F, 980.0F, 1000.0F};
    const auto hr = heartRateFromIntervals(intervals, 4);
    const auto hrv = calculateHrv(intervals, 4);
    TEST_ASSERT_EQUAL(static_cast<int>(ValueStatus::Valid), static_cast<int>(hr.status));
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 60.0F, hr.value);
    TEST_ASSERT_EQUAL(static_cast<int>(ValueStatus::Valid), static_cast<int>(hrv.status));
    TEST_ASSERT_EQUAL_UINT16(4, hrv.interval_count);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ac_dc_estimator_fixed_vector);
    RUN_TEST(test_spo2_ratio_is_available_but_percent_is_unvalidated);
    RUN_TEST(test_hr_and_hrv_use_intervals);
    return UNITY_END();
}
