#include "metrics/hrv.h"

#include "config/thresholds.h"

#include <cmath>

namespace ppgfw {

HrvMetrics calculateHrv(const float* intervals_ms, size_t count) {
    HrvMetrics output{};
    if (intervals_ms == nullptr || count < 2) {
        return output;
    }
    double sum = 0.0;
    size_t valid_count = 0;
    for (size_t index = 0; index < count; ++index) {
        const float value = intervals_ms[index];
        if (value >= config::candidate::kMinimumBeatIntervalMs &&
            value <= config::candidate::kMaximumBeatIntervalMs) {
            sum += value;
            ++valid_count;
        }
    }
    if (valid_count < 2) {
        return output;
    }
    const double mean = sum / valid_count;
    double squared_deviation = 0.0;
    double squared_successive_difference = 0.0;
    size_t differences = 0;
    size_t nn50 = 0;
    float previous = 0.0F;
    bool have_previous = false;
    for (size_t index = 0; index < count; ++index) {
        const float value = intervals_ms[index];
        if (value < config::candidate::kMinimumBeatIntervalMs ||
            value > config::candidate::kMaximumBeatIntervalMs) {
            continue;
        }
        const double delta = value - mean;
        squared_deviation += delta * delta;
        if (have_previous) {
            const double difference = value - previous;
            squared_successive_difference += difference * difference;
            if (std::fabs(difference) > 50.0) {
                ++nn50;
            }
            ++differences;
        }
        previous = value;
        have_previous = true;
    }
    output.status = ValueStatus::Valid;
    output.mean_interval_ms = static_cast<float>(mean);
    output.sdnn_ms = static_cast<float>(std::sqrt(squared_deviation / (valid_count - 1U)));
    output.rmssd_ms = differences > 0
                          ? static_cast<float>(std::sqrt(squared_successive_difference / differences))
                          : kUnavailableValue;
    output.pnn50_percent = differences > 0
                               ? 100.0F * static_cast<float>(nn50) / static_cast<float>(differences)
                               : kUnavailableValue;
    output.interval_count = static_cast<uint16_t>(valid_count);
    return output;
}

}

