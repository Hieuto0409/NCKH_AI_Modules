#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include <stdint.h>

#include "biquad.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Machine-readable reasons for a window that should not be trusted. */
#define SP_REASON_LOW_RED_PERFUSION (1UL << 0)
#define SP_REASON_LOW_IR_PERFUSION  (1UL << 1)
#define SP_REASON_LOW_ECG_MORPH     (1UL << 2)
#define SP_REASON_LOW_ECG_QRS       (1UL << 3)
#define SP_REASON_TIMESTAMP_GAP     (1UL << 4)
#define SP_REASON_CONFIG_CLAMPED    (1UL << 5)

typedef struct {
    /* Per-sample outputs. */
    float ecg_filtered;
    float ecg_qrs;
    float red_ac;
    float ir_ac;
    float red_dc;
    float ir_dc;

    /* Window outputs; meaningful when window_ready == 1. */
    uint8_t window_ready;
    uint32_t window_index;
    float red_perfusion;
    float ir_perfusion;
    float ecg_rms_morph;
    float ecg_rms_qrs;
    uint8_t ecg_good;
    uint8_t ppg_good;
    uint8_t both_good;
    uint32_t quality_reason_mask;
    uint32_t window_gap_count;

    /* Timing diagnostics. */
    uint8_t gap_detected;
    uint32_t gap_count;
    uint32_t max_gap_us;
} SignalResult;

typedef struct {
    float fs_hz;
    uint32_t sample_index;
    uint32_t window_samples;
    uint32_t step_samples;
    uint32_t ring_pos;
    uint32_t stat_count;
    uint32_t window_index;

    uint32_t last_timestamp_us;
    uint8_t have_timestamp;
    uint32_t gap_count;
    uint32_t max_gap_us;
    uint8_t config_clamped;

    BiquadCascade ecg_morph;
    BiquadCascade ecg_qrs;
    BiquadCascade red_ac;
    BiquadCascade ir_ac;
    BiquadCascade red_dc;
    BiquadCascade ir_dc;

    /* One record per sample in the current sliding window. */
    struct {
        float red_ac;
        float red_dc;
        float ir_ac;
        float ir_dc;
        float ecg_morph;
        float ecg_qrs;
        uint8_t gap;
    } ring[MAX_WINDOW_SAMPLES];

    double red_ac_sum;
    double red_ac_sum_sq;
    double red_dc_sum;
    double ir_ac_sum;
    double ir_ac_sum_sq;
    double ir_dc_sum;
    double ecg_morph_sum_sq;
    double ecg_qrs_sum_sq;
    uint32_t window_gap_count;
} SignalProcessor;

void sp_init(SignalProcessor *sp, float fs_hz);
void sp_reset(SignalProcessor *sp);

int sp_process_sample(SignalProcessor *sp,
                      float ecg_raw,
                      float red_raw,
                      float ir_raw,
                      uint32_t timestamp_us,
                      SignalResult *result);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_PROCESSOR_H */
