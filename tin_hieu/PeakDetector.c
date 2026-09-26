#include "PeakDetector.h"

#include <math.h>
#include <stddef.h>

static uint32_t seconds_to_samples(float fs_hz, float seconds)
{
    float value = fs_hz * seconds;
    if (value < 1.0f) return 1U;
    return (uint32_t)(value + 0.5f);
}

static uint32_t previous_index(uint32_t index, uint32_t offset)
{
    return (index + PEAK_HISTORY_CAPACITY -
            (offset % PEAK_HISTORY_CAPACITY)) % PEAK_HISTORY_CAPACITY;
}

static float oriented(const PeakDetector *detector, float value)
{
    return (detector->polarity < 0) ? -value : value;
}

int pd_init(PeakDetector *detector,
            float fs_hz,
            float lookahead_seconds,
            float refractory_seconds,
            float min_prominence,
            int8_t polarity)
{
    uint32_t radius;
    uint32_t refractory;

    if (detector == NULL || fs_hz <= 0.0f || lookahead_seconds <= 0.0f
        || refractory_seconds <= 0.0f || min_prominence < 0.0f) {
        return -1;
    }

    detector->fs_hz = fs_hz;
    detector->config_clamped = 0U;
    radius = seconds_to_samples(fs_hz, lookahead_seconds);
    refractory = seconds_to_samples(fs_hz, refractory_seconds);
    if (radius > PEAK_MAX_RADIUS_SAMPLES) {
        radius = PEAK_MAX_RADIUS_SAMPLES;
        detector->config_clamped = 1U;
    }
    if (refractory > PEAK_HISTORY_CAPACITY - 1U) {
        refractory = PEAK_HISTORY_CAPACITY - 1U;
        detector->config_clamped = 1U;
    }
    detector->radius_samples = radius;
    detector->refractory_samples = refractory;
    detector->min_prominence = min_prominence;
    detector->polarity = (polarity < 0) ? -1 : 1;
    pd_reset(detector);
    return 0;
}

void pd_reset(PeakDetector *detector)
{
    if (detector == NULL) return;
    detector->write_pos = 0U;
    detector->count = 0U;
    detector->last_peak_index = 0U;
    detector->have_last_peak = 0U;
    detector->last_input_index = 0U;
    detector->have_input_index = 0U;
}

int pd_process(PeakDetector *detector,
               float sample,
               uint32_t sample_index,
               PeakEvent *event)
{
    uint32_t candidate_pos;
    uint32_t candidate_index;
    float candidate;
    float neighbour_sum = 0.0f;
    uint32_t neighbour_count = 0U;
    uint32_t offset;

    if (detector == NULL || event == NULL) return -1;
    event->ready = 0U;
    event->sample_index = 0U;
    event->value = 0.0f;
    if (!isfinite(sample)) return -2;
    if (detector->have_input_index != 0U
        && sample_index <= detector->last_input_index) {
        return -3;
    }
    detector->last_input_index = sample_index;
    detector->have_input_index = 1U;

    detector->history[detector->write_pos] = sample;
    detector->indices[detector->write_pos] = sample_index;
    detector->write_pos = (detector->write_pos + 1U) % PEAK_HISTORY_CAPACITY;
    if (detector->count < PEAK_HISTORY_CAPACITY) detector->count++;

    if (detector->count < (2U * detector->radius_samples + 1U)) return 0;

    candidate_pos = previous_index(detector->write_pos,
                                   detector->radius_samples + 1U);
    candidate_index = detector->indices[candidate_pos];
    candidate = oriented(detector, detector->history[candidate_pos]);

    for (offset = 1U; offset <= detector->radius_samples; ++offset) {
        uint32_t left = previous_index(candidate_pos, offset);
        uint32_t right = (candidate_pos + offset) % PEAK_HISTORY_CAPACITY;
        float left_value = oriented(detector, detector->history[left]);
        float right_value = oriented(detector, detector->history[right]);
        if (candidate <= left_value || candidate < right_value) return 0;
        neighbour_sum += left_value + right_value;
        neighbour_count += 2U;
    }

    if (neighbour_count == 0U) return 0;
    if ((candidate - (neighbour_sum / (float)neighbour_count))
        < detector->min_prominence) return 0;

    if (detector->have_last_peak
        && (candidate_index - detector->last_peak_index)
           < detector->refractory_samples) {
        return 0;
    }

    detector->last_peak_index = candidate_index;
    detector->have_last_peak = 1U;
    event->ready = 1U;
    event->sample_index = candidate_index;
    event->value = detector->history[candidate_pos];
    return 0;
}
