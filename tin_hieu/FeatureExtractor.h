#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FEATURE_MAX_PEAKS 256U

typedef struct {
    float fs_hz;
    uint16_t min_valid_intervals;
    uint32_t ecg_peaks[FEATURE_MAX_PEAKS];
    uint32_t bvp_peaks[FEATURE_MAX_PEAKS];
    size_t ecg_count;
    size_t bvp_count;
} FeatureAccumulator;

typedef struct {
    uint8_t feature_valid;
    uint16_t ecg_valid_intervals;
    uint16_t bvp_valid_intervals;
    float ecg_hr_median_bpm;
    float ecg_rr_median_ms;
    float ecg_sdrr_ms;
    float ecg_rmssd_ms;
    float bvp_pr_median_bpm;
    float bvp_ppi_median_ms;
    float bvp_sdppi_ms;
    float bvp_rmssd_ms;
} FeatureSet;

int fx_init(FeatureAccumulator *accumulator,
            float fs_hz,
            uint16_t min_valid_intervals);
void fx_reset(FeatureAccumulator *accumulator);
int fx_add_ecg_peak(FeatureAccumulator *accumulator, uint32_t sample_index);
int fx_add_bvp_peak(FeatureAccumulator *accumulator, uint32_t sample_index);
int fx_compute(const FeatureAccumulator *accumulator, FeatureSet *features);

#ifdef __cplusplus
}
#endif

#endif /* FEATURE_EXTRACTOR_H */
