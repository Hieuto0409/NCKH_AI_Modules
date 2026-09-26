#pragma once

#include "dsp/filters.h"
#include "types/raw_samples.h"

namespace ppgfw {

struct PpgProcessedSample {
    uint64_t timestamp_us{};
    float red_ac{};
    float ir_ac{};
    float red_dc{};
    float ir_dc{};
    uint16_t flags{};
};

class PpgPreprocessor {
public:
    PpgPreprocessor();
    void reset();
    PpgProcessedSample process(const PpgSample& sample);

private:
    ExponentialDcBlocker red_dc_blocker_;
    ExponentialDcBlocker ir_dc_blocker_;
    ExponentialLowPass red_low_pass_;
    ExponentialLowPass ir_low_pass_;
};

}

