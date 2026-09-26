#include "acquisition/timestamp_service.h"

#if defined(PPGFW_NATIVE_TEST)
#include <chrono>
#else
#include <esp_timer.h>
#endif

namespace ppgfw {

BatchTimestampPlan PpgTimestampReconstructor::plan(uint64_t poll_timestamp_us, size_t sample_count,
                                                    uint64_t last_timestamp_us,
                                                    uint64_t sample_period_us) {
    if (sample_count == 0 || sample_period_us == 0) {
        return {};
    }
    const uint64_t batch_span = static_cast<uint64_t>(sample_count - 1U) * sample_period_us;
    uint64_t first = poll_timestamp_us > batch_span ? poll_timestamp_us - batch_span : 0;
    // Cadence follows samples, not host polling jitter. The acquisition layer
    // resets last_timestamp_us after an explicit FIFO/sequence discontinuity.
    if (last_timestamp_us > 0) first = last_timestamp_us + sample_period_us;
    return {first, 0};
}

uint64_t TimestampService::nowUs() {
#if defined(PPGFW_NATIVE_TEST)
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#else
    return static_cast<uint64_t>(esp_timer_get_time());
#endif
}

}
