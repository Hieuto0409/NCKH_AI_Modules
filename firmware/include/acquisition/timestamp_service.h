#pragma once

#include <cstddef>
#include <cstdint>

namespace ppgfw {

struct BatchTimestampPlan {
    uint64_t first_timestamp_us{};
    uint32_t inferred_gap_samples{};
};

class PpgTimestampReconstructor {
public:
    static BatchTimestampPlan plan(uint64_t poll_timestamp_us, size_t sample_count,
                                   uint64_t last_timestamp_us, uint64_t sample_period_us);
};

class TimestampService {
public:
    static uint64_t nowUs();
};

}

