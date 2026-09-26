#include "SignalProcessor.h"

#include <math.h>
#include <stddef.h>

static void clear_stats(SignalProcessor *sp)
{
    sp->red_ac_sum = 0.0;
    sp->red_ac_sum_sq = 0.0;
    sp->red_dc_sum = 0.0;
    sp->ir_ac_sum = 0.0;
    sp->ir_ac_sum_sq = 0.0;
    sp->ir_dc_sum = 0.0;
    sp->ecg_morph_sum_sq = 0.0;
    sp->ecg_qrs_sum_sq = 0.0;
    sp->window_gap_count = 0U;
    sp->stat_count = 0U;
    sp->ring_pos = 0U;
}

static void subtract_record(SignalProcessor *sp, uint32_t index)
{
    const float r_ac = sp->ring[index].red_ac;
    const float r_dc = sp->ring[index].red_dc;
    const float i_ac = sp->ring[index].ir_ac;
    const float i_dc = sp->ring[index].ir_dc;
    const float e_m = sp->ring[index].ecg_morph;
    const float e_q = sp->ring[index].ecg_qrs;

    sp->red_ac_sum -= (double)r_ac;
    sp->red_ac_sum_sq -= (double)r_ac * (double)r_ac;
    sp->red_dc_sum -= (double)r_dc;
    sp->ir_ac_sum -= (double)i_ac;
    sp->ir_ac_sum_sq -= (double)i_ac * (double)i_ac;
    sp->ir_dc_sum -= (double)i_dc;
    sp->ecg_morph_sum_sq -= (double)e_m * (double)e_m;
    sp->ecg_qrs_sum_sq -= (double)e_q * (double)e_q;
    if (sp->ring[index].gap != 0U && sp->window_gap_count > 0U) {
        sp->window_gap_count--;
    }
}

static void add_record(SignalProcessor *sp, uint32_t index,
                       float r_ac, float r_dc, float i_ac, float i_dc,
                       float e_m, float e_q, uint8_t gap)
{
    sp->ring[index].red_ac = r_ac;
    sp->ring[index].red_dc = r_dc;
    sp->ring[index].ir_ac = i_ac;
    sp->ring[index].ir_dc = i_dc;
    sp->ring[index].ecg_morph = e_m;
    sp->ring[index].ecg_qrs = e_q;
    sp->ring[index].gap = gap;

    sp->red_ac_sum += (double)r_ac;
    sp->red_ac_sum_sq += (double)r_ac * (double)r_ac;
    sp->red_dc_sum += (double)r_dc;
    sp->ir_ac_sum += (double)i_ac;
    sp->ir_ac_sum_sq += (double)i_ac * (double)i_ac;
    sp->ir_dc_sum += (double)i_dc;
    sp->ecg_morph_sum_sq += (double)e_m * (double)e_m;
    sp->ecg_qrs_sum_sq += (double)e_q * (double)e_q;
    if (gap != 0U) sp->window_gap_count++;
}

static void emit_result(SignalProcessor *sp, SignalResult *result)
{
    uint32_t n = sp->stat_count;
    float red_perf = 0.0f;
    float ir_perf = 0.0f;
    float ecg_morph_rms = 0.0f;
    float ecg_qrs_rms = 0.0f;

    if (n > 0U) {
        double inv_n = 1.0 / (double)n;
        float r_mean = (float)(sp->red_ac_sum * inv_n);
        float i_mean = (float)(sp->ir_ac_sum * inv_n);
        float r_dc = (float)(sp->red_dc_sum * inv_n);
        float i_dc = (float)(sp->ir_dc_sum * inv_n);
        float r_var = (float)(sp->red_ac_sum_sq * inv_n)
                    - r_mean * r_mean;
        float i_var = (float)(sp->ir_ac_sum_sq * inv_n)
                    - i_mean * i_mean;

        if (r_var < 0.0f) r_var = 0.0f;
        if (i_var < 0.0f) i_var = 0.0f;

        red_perf = sqrtf(r_var) / (fabsf(r_dc) + 1.0e-9f);
        ir_perf = sqrtf(i_var) / (fabsf(i_dc) + 1.0e-9f);
        ecg_morph_rms = sqrtf((float)(sp->ecg_morph_sum_sq * inv_n));
        ecg_qrs_rms = sqrtf((float)(sp->ecg_qrs_sum_sq * inv_n));
    }

    result->red_perfusion = red_perf;
    result->ir_perfusion = ir_perf;
    result->ecg_rms_morph = ecg_morph_rms;
    result->ecg_rms_qrs = ecg_qrs_rms;

    /* Mức A: perfusion gates PPG; RMS floors are an ECG placeholder. */
    result->ppg_good = (uint8_t)((red_perf >= PERFUSION_MIN_RED)
                               && (ir_perf >= PERFUSION_MIN_IR));
    result->ecg_good = (uint8_t)((ecg_morph_rms >= ECG_RMS_MORPH_MIN)
                               && (ecg_qrs_rms >= ECG_RMS_QRS_MIN));
    result->both_good = (uint8_t)(result->ppg_good && result->ecg_good);
    result->quality_reason_mask = 0UL;
    if (red_perf < PERFUSION_MIN_RED) {
        result->quality_reason_mask |= SP_REASON_LOW_RED_PERFUSION;
    }
    if (ir_perf < PERFUSION_MIN_IR) {
        result->quality_reason_mask |= SP_REASON_LOW_IR_PERFUSION;
    }
    if (ecg_morph_rms < ECG_RMS_MORPH_MIN) {
        result->quality_reason_mask |= SP_REASON_LOW_ECG_MORPH;
    }
    if (ecg_qrs_rms < ECG_RMS_QRS_MIN) {
        result->quality_reason_mask |= SP_REASON_LOW_ECG_QRS;
    }
    if (sp->window_gap_count > 0U) {
        result->quality_reason_mask |= SP_REASON_TIMESTAMP_GAP;
    }
    if (sp->config_clamped != 0U) {
        result->quality_reason_mask |= SP_REASON_CONFIG_CLAMPED;
    }
    result->window_gap_count = sp->window_gap_count;
    result->window_index = sp->window_index++;
}

void sp_init(SignalProcessor *sp, float fs_hz)
{
    uint32_t requested_window;

    if (sp == NULL) return;
    if (fs_hz <= 0.0f) fs_hz = DEFAULT_FS_HZ;

    sp->fs_hz = fs_hz;
    sp->sample_index = 0U;
    sp->window_index = 0U;
    sp->last_timestamp_us = 0U;
    sp->have_timestamp = 0U;
    sp->gap_count = 0U;
    sp->max_gap_us = 0U;
    sp->config_clamped = 0U;
    sp->window_gap_count = 0U;

    requested_window = (uint32_t)(fs_hz * WINDOW_SECONDS + 0.5f);
    sp->window_samples = requested_window;
    sp->step_samples = (uint32_t)(fs_hz * STEP_SECONDS + 0.5f);

    if (sp->window_samples == 0U) sp->window_samples = 1U;
    if (sp->step_samples == 0U) sp->step_samples = 1U;
    if (sp->window_samples > MAX_WINDOW_SAMPLES) {
        sp->window_samples = MAX_WINDOW_SAMPLES;
        sp->config_clamped = 1U;
    }
    if (sp->step_samples > sp->window_samples) {
        sp->step_samples = sp->window_samples;
    }

    design_butterworth_bp(&sp->ecg_morph, fs_hz,
                          ECG_HPF_HZ, ECG_LPF_HZ, 4);
    design_butterworth_bp(&sp->ecg_qrs, fs_hz,
                          ECG_QRS_LO_HZ, ECG_QRS_HI_HZ, 4);
    design_butterworth_bp(&sp->red_ac, fs_hz,
                          PPG_HPF_HZ, PPG_LPF_HZ, 4);
    design_butterworth_bp(&sp->ir_ac, fs_hz,
                          PPG_HPF_HZ, PPG_LPF_HZ, 4);
    design_butterworth_lp(&sp->red_dc, fs_hz, PPG_DC_LPF_HZ, 4);
    design_butterworth_lp(&sp->ir_dc, fs_hz, PPG_DC_LPF_HZ, 4);

    clear_stats(sp);
}

void sp_reset(SignalProcessor *sp)
{
    if (sp == NULL) return;
    cascade_reset(&sp->ecg_morph);
    cascade_reset(&sp->ecg_qrs);
    cascade_reset(&sp->red_ac);
    cascade_reset(&sp->ir_ac);
    cascade_reset(&sp->red_dc);
    cascade_reset(&sp->ir_dc);
    sp->sample_index = 0U;
    sp->window_index = 0U;
    sp->last_timestamp_us = 0U;
    sp->have_timestamp = 0U;
    sp->gap_count = 0U;
    sp->max_gap_us = 0U;
    clear_stats(sp);
}

int sp_process_sample(SignalProcessor *sp,
                      float ecg_raw,
                      float red_raw,
                      float ir_raw,
                      uint32_t timestamp_us,
                      SignalResult *result)
{
    uint32_t expected_period_us;
    uint32_t dt_us;
    uint32_t pos;
    float ecg_m;
    float ecg_q;
    float r_ac;
    float i_ac;
    float r_dc;
    float i_dc;

    if (sp == NULL || result == NULL) return -1;

    result->window_ready = 0U;
    result->gap_detected = 0U;
    result->gap_count = sp->gap_count;
    result->max_gap_us = sp->max_gap_us;
    result->quality_reason_mask = 0UL;
    result->window_gap_count = 0U;

    /* Timestamp diagnostics.  The timestamp is intentionally not discarded. */
    if (sp->have_timestamp) {
        dt_us = timestamp_us - sp->last_timestamp_us;
        expected_period_us = (uint32_t)(1000000.0f / sp->fs_hz + 0.5f);
        /* Unsigned subtraction handles a normal single micros() wrap. This
         * extra guard only rejects an implausible discontinuity above 4000 s,
         * so it cannot become a false gap/max-gap value. */
        if (dt_us > 4000000000U) {
            dt_us = 0U;
        } else if (dt_us > (2U * expected_period_us)) {
            sp->gap_count++;
            result->gap_detected = 1U;
        }
        if (dt_us > sp->max_gap_us && dt_us < 4000000000U) {
            sp->max_gap_us = dt_us;
        }
    }
    sp->last_timestamp_us = timestamp_us;
    sp->have_timestamp = 1U;

    /* Six independent realtime paths. */
    ecg_m = cascade_step(&sp->ecg_morph, ecg_raw);
    ecg_q = cascade_step(&sp->ecg_qrs, ecg_raw);
    r_ac = cascade_step(&sp->red_ac, red_raw);
    i_ac = cascade_step(&sp->ir_ac, ir_raw);
    r_dc = cascade_step(&sp->red_dc, red_raw);
    i_dc = cascade_step(&sp->ir_dc, ir_raw);

    result->ecg_filtered = ecg_m;
    result->ecg_qrs = ecg_q;
    result->red_ac = r_ac;
    result->ir_ac = i_ac;
    result->red_dc = r_dc;
    result->ir_dc = i_dc;

    /* Correct overlapping 5 s windows: remove the old ring item first. */
    pos = sp->ring_pos;
    if (sp->stat_count >= sp->window_samples) {
        subtract_record(sp, pos);
    } else {
        sp->stat_count++;
    }
    add_record(sp, pos, r_ac, r_dc, i_ac, i_dc, ecg_m, ecg_q,
               result->gap_detected);
    sp->ring_pos = (pos + 1U) % sp->window_samples;

    sp->sample_index++;
    if (sp->sample_index >= sp->window_samples) {
        uint32_t elapsed = sp->sample_index - sp->window_samples;
        if ((elapsed % sp->step_samples) == 0U) {
            emit_result(sp, result);
            result->window_ready = 1U;
        }
    }

    result->gap_count = sp->gap_count;
    result->max_gap_us = sp->max_gap_us;
    return 0;
}
