#include "FeatureExtractor.h"

#include <math.h>
#include <stddef.h>

#define RR_MIN_SECONDS 0.30f
#define RR_MAX_SECONDS 2.00f

static void set_nan_features(FeatureSet *features)
{
    features->feature_valid = 0U;
    features->ecg_valid_intervals = 0U;
    features->bvp_valid_intervals = 0U;
    features->ecg_hr_median_bpm = NAN;
    features->ecg_rr_median_ms = NAN;
    features->ecg_sdrr_ms = NAN;
    features->ecg_rmssd_ms = NAN;
    features->bvp_pr_median_bpm = NAN;
    features->bvp_ppi_median_ms = NAN;
    features->bvp_sdppi_ms = NAN;
    features->bvp_rmssd_ms = NAN;
}

static int append_peak(uint32_t *peaks, size_t *count, uint32_t sample_index)
{
    if (peaks == NULL || count == NULL) return -1;
    if (*count > 0U && sample_index <= peaks[*count - 1U]) return -2;
    if (*count >= FEATURE_MAX_PEAKS) return -3;
    peaks[*count] = sample_index;
    (*count)++;
    return 0;
}

static size_t intervals_ms(const uint32_t *peaks,
                           size_t peak_count,
                           float fs_hz,
                           float *intervals,
                           uint8_t *valid)
{
    size_t i;
    size_t valid_count = 0U;
    for (i = 0U; i + 1U < peak_count; ++i) {
        float seconds = (float)(peaks[i + 1U] - peaks[i]) / fs_hz;
        intervals[i] = seconds * 1000.0f;
        valid[i] = (uint8_t)(seconds >= RR_MIN_SECONDS
                           && seconds <= RR_MAX_SECONDS);
        if (valid[i] != 0U) valid_count++;
    }
    return valid_count;
}

static void sort_float(float *values, size_t count)
{
    size_t i;
    for (i = 1U; i < count; ++i) {
        float key = values[i];
        size_t j = i;
        while (j > 0U && values[j - 1U] > key) {
            values[j] = values[j - 1U];
            --j;
        }
        values[j] = key;
    }
}

static float median_valid(const float *intervals,
                          const uint8_t *valid,
                          size_t interval_count,
                          uint16_t *valid_count_out)
{
    float values[FEATURE_MAX_PEAKS];
    size_t count = 0U;
    size_t i;
    if (valid_count_out != NULL) *valid_count_out = 0U;
    for (i = 0U; i < interval_count; ++i) {
        if (valid[i] != 0U) {
            values[count++] = intervals[i];
        }
    }
    if (valid_count_out != NULL) *valid_count_out = (uint16_t)count;
    if (count == 0U) return NAN;
    sort_float(values, count);
    if ((count % 2U) != 0U) return values[count / 2U];
    return 0.5f * (values[count / 2U - 1U] + values[count / 2U]);
}

static float sample_std_valid(const float *intervals,
                              const uint8_t *valid,
                              size_t interval_count,
                              uint16_t valid_count)
{
    double sum = 0.0;
    double sum_sq = 0.0;
    size_t i;
    if (valid_count < 2U) return NAN;
    for (i = 0U; i < interval_count; ++i) {
        if (valid[i] != 0U) {
            double value = (double)intervals[i];
            sum += value;
            sum_sq += value * value;
        }
    }
    {
        double n = (double)valid_count;
        double variance = (sum_sq - (sum * sum / n)) / (n - 1.0);
        if (variance < 0.0) variance = 0.0;
        return sqrtf((float)variance);
    }
}

static float adjacent_rmssd(const float *intervals,
                            const uint8_t *valid,
                            size_t interval_count)
{
    double sum_sq = 0.0;
    size_t pair_count = 0U;
    size_t i;
    for (i = 0U; i + 1U < interval_count; ++i) {
        /* Both intervals must be valid and adjacent in the original peak
         * sequence. This prevents bridging over an invalid interval. */
        if (valid[i] != 0U && valid[i + 1U] != 0U) {
            double diff = (double)intervals[i + 1U]
                        - (double)intervals[i];
            sum_sq += diff * diff;
            pair_count++;
        }
    }
    if (pair_count == 0U) return NAN;
    return sqrtf((float)(sum_sq / (double)pair_count));
}

int fx_init(FeatureAccumulator *accumulator,
            float fs_hz,
            uint16_t min_valid_intervals)
{
    if (accumulator == NULL || fs_hz <= 0.0f || min_valid_intervals < 2U) {
        return -1;
    }
    accumulator->fs_hz = fs_hz;
    accumulator->min_valid_intervals = min_valid_intervals;
    fx_reset(accumulator);
    return 0;
}

void fx_reset(FeatureAccumulator *accumulator)
{
    if (accumulator == NULL) return;
    accumulator->ecg_count = 0U;
    accumulator->bvp_count = 0U;
}

int fx_add_ecg_peak(FeatureAccumulator *accumulator, uint32_t sample_index)
{
    if (accumulator == NULL) return -1;
    return append_peak(accumulator->ecg_peaks,
                       &accumulator->ecg_count, sample_index);
}

int fx_add_bvp_peak(FeatureAccumulator *accumulator, uint32_t sample_index)
{
    if (accumulator == NULL) return -1;
    return append_peak(accumulator->bvp_peaks,
                       &accumulator->bvp_count, sample_index);
}

static void compute_channel(const uint32_t *peaks,
                            size_t peak_count,
                            float fs_hz,
                            uint16_t *valid_count,
                            float *median_ms,
                            float *sd_ms,
                            float *rmssd_ms)
{
    float intervals[FEATURE_MAX_PEAKS];
    uint8_t valid[FEATURE_MAX_PEAKS];
    size_t interval_count = 0U;
    *valid_count = 0U;
    *median_ms = NAN;
    *sd_ms = NAN;
    *rmssd_ms = NAN;
    if (peak_count > 1U) interval_count = peak_count - 1U;
    if (interval_count == 0U) return;
    (void)intervals_ms(peaks, peak_count, fs_hz, intervals, valid);
    *median_ms = median_valid(intervals, valid, interval_count, valid_count);
    *sd_ms = sample_std_valid(intervals, valid, interval_count,
                              *valid_count);
    *rmssd_ms = adjacent_rmssd(intervals, valid, interval_count);
}

int fx_compute(const FeatureAccumulator *accumulator, FeatureSet *features)
{
    float ecg_hr;
    float bvp_pr;
    if (accumulator == NULL || features == NULL) return -1;
    set_nan_features(features);
    compute_channel(accumulator->ecg_peaks, accumulator->ecg_count,
                    accumulator->fs_hz, &features->ecg_valid_intervals,
                    &features->ecg_rr_median_ms, &features->ecg_sdrr_ms,
                    &features->ecg_rmssd_ms);
    compute_channel(accumulator->bvp_peaks, accumulator->bvp_count,
                    accumulator->fs_hz, &features->bvp_valid_intervals,
                    &features->bvp_ppi_median_ms, &features->bvp_sdppi_ms,
                    &features->bvp_rmssd_ms);
    if (isfinite(features->ecg_rr_median_ms)
        && features->ecg_rr_median_ms > 0.0f) {
        ecg_hr = 60000.0f / features->ecg_rr_median_ms;
        features->ecg_hr_median_bpm = ecg_hr;
    }
    if (isfinite(features->bvp_ppi_median_ms)
        && features->bvp_ppi_median_ms > 0.0f) {
        bvp_pr = 60000.0f / features->bvp_ppi_median_ms;
        features->bvp_pr_median_bpm = bvp_pr;
    }
    features->feature_valid = (uint8_t)(
        features->ecg_valid_intervals >= accumulator->min_valid_intervals
        && features->bvp_valid_intervals >= accumulator->min_valid_intervals);
    return 0;
}
