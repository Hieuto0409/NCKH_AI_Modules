#include <math.h>
#include <stdio.h>

#include "SignalProcessor.h"

int main(void)
{
    SignalProcessor processor;
    SignalResult result;
    SignalProcessor gap_processor;
    SignalResult gap_result;
    unsigned int ready_count = 0U;
    unsigned int gap_reason_seen = 0U;
    unsigned int i;
    const float fs = 100.0f;
    const float pi2 = 6.28318530718f;

    sp_init(&processor, fs);

    /* 15 s: should emit at 5.0, 7.5, ..., 15.0 s => 5 windows. */
    for (i = 0U; i < 1500U; ++i) {
        float t = (float)i / fs;
        float ecg = 2048.0f + 120.0f * sinf(pi2 * 1.2f * t);
        float red = 100000.0f + 100.0f * sinf(pi2 * 1.2f * t);
        float ir = 80000.0f + 50.0f * sinf(pi2 * 1.2f * t);
        uint32_t timestamp = (uint32_t)((float)i * 1000000.0f / fs);

        if (sp_process_sample(&processor, ecg, red, ir,
                              timestamp, &result) != 0) {
            fprintf(stderr, "process failed\n");
            return 1;
        }
        if (result.window_ready != 0U) {
            ++ready_count;
            printf("window %u: red=%.7f ir=%.7f ppg=%u\n",
                   result.window_index,
                   result.red_perfusion,
                   result.ir_perfusion,
                   result.ppg_good);
        }
    }

    if (ready_count != 5U) {
        fprintf(stderr, "expected 5 windows, got %u\n", ready_count);
        return 2;
    }
    if (processor.config_clamped != 0U) {
        fprintf(stderr, "unexpected window clamp\n");
        return 3;
    }

    /* A long timestamp pause must be visible in the window reason mask. */
    sp_init(&gap_processor, fs);
    for (i = 0U; i < 700U; ++i) {
        float t = (float)i / fs;
        uint32_t timestamp = (uint32_t)((float)i * 1000000.0f / fs);
        if (i >= 300U) timestamp += 100000U;
        if (sp_process_sample(&gap_processor,
                              sinf(pi2 * 1.2f * t),
                              100000.0f + 100.0f * sinf(pi2 * 1.2f * t),
                              80000.0f + 50.0f * sinf(pi2 * 1.2f * t),
                              timestamp, &gap_result) != 0) {
            fprintf(stderr, "gap process failed\n");
            return 4;
        }
        if (gap_result.window_ready != 0U
            && (gap_result.quality_reason_mask & SP_REASON_TIMESTAMP_GAP) != 0UL) {
            gap_reason_seen = 1U;
        }
    }
    if (gap_reason_seen == 0U) {
        fprintf(stderr, "timestamp gap was not reported\n");
        return 5;
    }

    /* A uint32_t micros() wrap must not be counted as a gap. */
    {
        SignalProcessor wrap_processor;
        SignalResult wrap_result;
        sp_init(&wrap_processor, fs);
        if (sp_process_sample(&wrap_processor, 2048.0f, 100000.0f,
                              80000.0f, 4294960000U, &wrap_result) != 0
            || sp_process_sample(&wrap_processor, 2048.0f, 100000.0f,
                                 80000.0f, 5000U, &wrap_result) != 0) {
            fprintf(stderr, "timestamp wrap process failed\n");
            return 6;
        }
        if (wrap_processor.gap_count != 0U) {
            fprintf(stderr, "timestamp wrap was counted as a gap\n");
            return 7;
        }
    }

    /* High-pass branch should settle to approximately zero on DC input. */
    {
        SignalProcessor dc_processor;
        SignalResult dc_result;
        float last = 0.0f;
        sp_init(&dc_processor, fs);
        for (i = 0U; i < 500U; ++i) {
            if (sp_process_sample(&dc_processor, 2048.0f, 100000.0f,
                                  80000.0f, i * 10000U, &dc_result) != 0) {
                fprintf(stderr, "DC filter process failed\n");
                return 8;
            }
            last = dc_result.ecg_filtered;
        }
        if (fabsf(last) > 5.0f) {
            fprintf(stderr, "HP did not reject DC: last=%.4f\n", last);
            return 9;
        }
        printf("HP DC reject: last_ecg=%.6f OK\n", last);
    }

    /* At fs=500 Hz, 240 Hz is near Nyquist and well above the 35 Hz cutoff. */
    {
        SignalProcessor tone_processor;
        SignalResult tone_result;
        float max_e = 0.0f;
        const float fs_500 = 500.0f;
        sp_init(&tone_processor, fs_500);
        for (i = 0U; i < 1500U; ++i) {
            float t = (float)i / fs_500;
            float input = 1000.0f * sinf(pi2 * 240.0f * t);
            uint32_t timestamp = i * 2000U;
            if (sp_process_sample(&tone_processor, input, 100000.0f,
                                  80000.0f, timestamp, &tone_result) != 0) {
                fprintf(stderr, "high-frequency filter process failed\n");
                return 10;
            }
            if (i > 500U && fabsf(tone_result.ecg_filtered) > max_e) {
                max_e = fabsf(tone_result.ecg_filtered);
            }
        }
        if (max_e > 100.0f) {
            fprintf(stderr, "LP did not reject high frequency: max=%.2f\n", max_e);
            return 11;
        }
        printf("LP high-frequency reject: max=%.2f OK\n", max_e);
    }
    printf("PASS: sliding-window and filter smoke test\n");
    return 0;
}
