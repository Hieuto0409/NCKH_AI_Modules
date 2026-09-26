#pragma once

#include "dsp/peak_detector_ppg.h"

namespace ppgfw {

class RPeakDetector {
public:
    void reset();
    PeakEvent update(float value, uint64_t timestamp_us);

private:
    float previous_two_{};
    float previous_one_{};
    uint64_t previous_one_timestamp_us_{};
    uint64_t last_peak_timestamp_us_{};
    float envelope_{};
    uint8_t samples_seen_{};
};

}

