#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed memory bound for causal look-ahead.  512 samples covers 0.3 s at
 * 1.6 kHz; larger requests are clamped and reported by config_clamped. */
#define PEAK_MAX_RADIUS_SAMPLES 512U
#define PEAK_HISTORY_CAPACITY (2U * PEAK_MAX_RADIUS_SAMPLES + 1U)

typedef struct {
    uint8_t ready;
    uint32_t sample_index;
    float value;
} PeakEvent;

typedef struct {
    float fs_hz;
    uint32_t radius_samples;
    uint32_t refractory_samples;
    float min_prominence;
    int8_t polarity;
    uint32_t write_pos;
    uint32_t count;
    uint32_t last_peak_index;
    uint8_t have_last_peak;
    uint32_t last_input_index;
    uint8_t have_input_index;
    uint8_t config_clamped;
    float history[PEAK_HISTORY_CAPACITY];
    uint32_t indices[PEAK_HISTORY_CAPACITY];
} PeakDetector;

int pd_init(PeakDetector *detector,
            float fs_hz,
            float lookahead_seconds,
            float refractory_seconds,
            float min_prominence,
            int8_t polarity);
void pd_reset(PeakDetector *detector);
int pd_process(PeakDetector *detector,
               float sample,
               uint32_t sample_index,
               PeakEvent *event);

#ifdef __cplusplus
}
#endif

#endif /* PEAK_DETECTOR_H */
