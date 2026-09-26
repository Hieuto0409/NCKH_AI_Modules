#include <unity.h>

#include "quality/ecg_sqi.h"
#include "quality/ppg_sqi.h"

using namespace ppgfw;

void test_clean_ppg_is_accepted() {
    PpgQualityInput input{};
    input.integrity = {100, 100, 0, 0, 0, 0, 0, 10, 100};
    input.signal.red_ac = 120.0F;
    input.signal.red_dc = 50000.0F;
    input.signal.ir_ac = 150.0F;
    input.signal.ir_dc = 60000.0F;
    input.signal.correlation = 0.9F;
    input.peak_count = 8;
    input.contact_seen = true;
    TEST_ASSERT_EQUAL(static_cast<int>(QualityStatus::Good),
                      static_cast<int>(PpgSqi::evaluate(input).status));
}

void test_clipped_ppg_is_rejected() {
    PpgQualityInput input{};
    input.integrity = {100, 100, 0, 0, 10, 0, 0, 10, 100};
    input.signal.red_ac = 120.0F;
    input.signal.red_dc = 50000.0F;
    input.signal.ir_ac = 150.0F;
    input.signal.ir_dc = 60000.0F;
    input.signal.correlation = 0.9F;
    input.peak_count = 8;
    input.contact_seen = true;
    const auto quality = PpgSqi::evaluate(input);
    TEST_ASSERT_EQUAL(static_cast<int>(QualityStatus::Poor), static_cast<int>(quality.status));
    TEST_ASSERT_BITS_HIGH(QualityClipping, quality.reasons);
}

void test_lead_off_ecg_is_rejected() {
    EcgQualityInput input{};
    input.integrity = {100, 100, 0, 0, 0, 1, 0, 10, 100};
    input.signal = {100, 0.0, 400.0, -50.0, 50.0};
    input.peak_count = 8;
    const auto quality = EcgSqi::evaluate(input);
    TEST_ASSERT_EQUAL(static_cast<int>(QualityStatus::Poor), static_cast<int>(quality.status));
    TEST_ASSERT_BITS_HIGH(QualityLeadOff, quality.reasons);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clean_ppg_is_accepted);
    RUN_TEST(test_clipped_ppg_is_rejected);
    RUN_TEST(test_lead_off_ecg_is_rejected);
    return UNITY_END();
}
