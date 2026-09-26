#include "metrics/heart_rate.h"

#include "config/thresholds.h"

#include <algorithm>
#include <array>

namespace ppgfw {

MetricValue heartRateFromIntervals(const float* intervals_ms, size_t count) {
    if (intervals_ms == nullptr || count == 0) {
        return {};
    }
    std::array<float, 256> valid{};
    size_t valid_count = 0;
    for (size_t index = 0; index < count && valid_count < valid.size(); ++index) {
        const float interval = intervals_ms[index];
        if (interval >= config::candidate::kMinimumBeatIntervalMs &&
            interval <= config::candidate::kMaximumBeatIntervalMs) {
            valid[valid_count++] = interval;
        }
    }
    if (valid_count == 0) {
        return {};
    }
    std::sort(valid.begin(), valid.begin() + valid_count);
    const float median = valid_count % 2 == 0
                             ? (valid[valid_count / 2U - 1U] + valid[valid_count / 2U]) * 0.5F
                             : valid[valid_count / 2U];
    return {ValueStatus::Valid, 60000.0F / median};
}

}

