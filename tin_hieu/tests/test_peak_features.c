#include <math.h>
#include <stdio.h>

#include "FeatureExtractor.h"
#include "PeakDetector.h"

static int test_peak_detector(void)
{
    PeakDetector detector;
    PeakEvent event;
    unsigned int events = 0U;
    uint32_t first_index = 0U;
    uint32_t last_index = 0U;
    unsigned int i;

    /* A clean 1 Hz waveform at 100 Hz: one positive maximum per second. */
    if (pd_init(&detector, 100.0f, 0.05f, 0.30f, 0.01f, 1) != 0) {
        fprintf(stderr, "peak init failed\n");
        return 1;
    }
    for (i = 0U; i < 900U; ++i) {
        float t = (float)i / 100.0f;
        float sample = 0.8f * sinf(6.28318530718f * t);
        if (pd_process(&detector, sample, i, &event) != 0) {
            fprintf(stderr, "peak process failed\n");
            return 2;
        }
        if (event.ready != 0U) {
            if (events == 0U) first_index = event.sample_index;
            last_index = event.sample_index;
            ++events;
        }
    }
    /* Edges are intentionally lost because this is a causal local detector. */
    if (events != 9U || first_index != 25U || last_index != 825U) {
        fprintf(stderr, "unexpected peaks: count=%u first=%u last=%u\n",
                events, first_index, last_index);
        return 3;
    }
    return 0;
}

static int test_feature_extractor(void)
{
    FeatureAccumulator accumulator;
    FeatureSet features;
    const uint32_t peaks[] = {0U, 100U, 200U, 500U, 600U, 700U};
    size_t i;

    /* 3 s interval is invalid. RMSSD must not bridge across it. */
    if (fx_init(&accumulator, 100.0f, 4U) != 0) {
        fprintf(stderr, "feature init failed\n");
        return 1;
    }
    for (i = 0U; i < sizeof(peaks) / sizeof(peaks[0]); ++i) {
        if (fx_add_ecg_peak(&accumulator, peaks[i]) != 0
            || fx_add_bvp_peak(&accumulator, peaks[i]) != 0) {
            fprintf(stderr, "feature peak append failed\n");
            return 2;
        }
    }
    if (fx_compute(&accumulator, &features) != 0) {
        fprintf(stderr, "feature compute failed\n");
        return 3;
    }
    if (features.feature_valid == 0U
        || features.ecg_valid_intervals != 4U
        || features.bvp_valid_intervals != 4U
        || fabsf(features.ecg_rr_median_ms - 1000.0f) > 0.001f
        || fabsf(features.ecg_hr_median_bpm - 60.0f) > 0.001f
        || fabsf(features.ecg_rmssd_ms) > 0.001f) {
        fprintf(stderr, "unexpected feature values: valid=%u n=%u/%u "
                        "rr=%.3f hr=%.3f rmssd=%.3f\n",
                features.feature_valid,
                features.ecg_valid_intervals,
                features.bvp_valid_intervals,
                features.ecg_rr_median_ms,
                features.ecg_hr_median_bpm,
                features.ecg_rmssd_ms);
        return 4;
    }
    return 0;
}

int main(void)
{
    if (test_peak_detector() != 0) return 1;
    if (test_feature_extractor() != 0) return 2;
    printf("PASS: causal peaks and interval features\n");
    return 0;
}
