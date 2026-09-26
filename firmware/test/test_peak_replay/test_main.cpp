#include <unity.h>

#include "dsp/peak_detector_ppg.h"
#include "dsp/rpeak_detector.h"

using namespace ppgfw;

void test_periodic_ppg_peaks_are_detected() {
    PpgPeakDetector detector;
    detector.reset();
    const float waveform[] = {0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10, 0};
    unsigned peaks = 0;
    for (unsigned i = 0; i < sizeof(waveform) / sizeof(waveform[0]); ++i) {
        peaks += detector.update(waveform[i], static_cast<uint64_t>(i) * 100000U).detected ? 1U : 0U;
    }
    TEST_ASSERT_EQUAL_UINT32(3, peaks);
}

void test_rpeak_detector_uses_absolute_amplitude() {
    RPeakDetector detector;
    detector.reset();
    const float waveform[] = {0, 0, -20, 0, 0, 0, -20, 0};
    unsigned peaks = 0;
    for (unsigned i = 0; i < sizeof(waveform) / sizeof(waveform[0]); ++i) {
        peaks += detector.update(waveform[i], static_cast<uint64_t>(i) * 100000U).detected ? 1U : 0U;
    }
    TEST_ASSERT_EQUAL_UINT32(2, peaks);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_periodic_ppg_peaks_are_detected);
    RUN_TEST(test_rpeak_detector_uses_absolute_amplitude);
    return UNITY_END();
}
