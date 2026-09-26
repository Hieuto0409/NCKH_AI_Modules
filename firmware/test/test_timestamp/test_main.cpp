#include <unity.h>

#include "acquisition/timestamp_service.h"

using ppgfw::PpgTimestampReconstructor;

void test_fifo_batch_is_backdated_from_poll_time() {
    const auto plan = PpgTimestampReconstructor::plan(1000000, 4, 0, 5000);
    TEST_ASSERT_EQUAL_UINT64(985000, plan.first_timestamp_us);
    TEST_ASSERT_EQUAL_UINT32(0, plan.inferred_gap_samples);
}

void test_poll_delay_preserves_grid() {
    const auto plan = PpgTimestampReconstructor::plan(1000000, 4, 950000, 5000);
    TEST_ASSERT_EQUAL_UINT64(955000, plan.first_timestamp_us);
    TEST_ASSERT_EQUAL_UINT32(0, plan.inferred_gap_samples);
}

void test_small_poll_delay_preserves_grid() {
    const auto plan = PpgTimestampReconstructor::plan(1000000, 4, 980000, 5000);
    TEST_ASSERT_EQUAL_UINT64(985000, plan.first_timestamp_us);
    TEST_ASSERT_EQUAL_UINT32(0, plan.inferred_gap_samples);

    const auto one_gap = PpgTimestampReconstructor::plan(1010000, 4, 985000, 5000);
    TEST_ASSERT_EQUAL_UINT64(990000, one_gap.first_timestamp_us);
    TEST_ASSERT_EQUAL_UINT32(0, one_gap.inferred_gap_samples);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fifo_batch_is_backdated_from_poll_time);
    RUN_TEST(test_poll_delay_preserves_grid);
    RUN_TEST(test_small_poll_delay_preserves_grid);
    return UNITY_END();
}
