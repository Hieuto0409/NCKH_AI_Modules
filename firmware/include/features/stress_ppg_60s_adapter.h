#pragma once

#include "config/sampling.h"
#include "types/model_feature_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ppgfw {

class StressPpg60sAdapter {
public:
    void reset(uint64_t window_start_us);
    void addPeak(uint64_t timestamp_us);
    StressPpgFeatureVector build(uint64_t window_end_us) const;

private:
    std::array<uint64_t, config::kIntervalCapacity> peak_timestamps_us_{};
    size_t peak_count_{};
    uint64_t window_start_us_{};
};

}
