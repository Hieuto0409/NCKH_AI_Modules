#pragma once

#include "dsp/filters.h"
#include "types/raw_samples.h"

namespace ppgfw {

struct EcgProcessedSample {
    uint64_t timestamp_us{};
    float value{};
    float baseline{};
    uint16_t flags{};
};

class EcgPreprocessor {
public:
    EcgPreprocessor();
    void reset();
    EcgProcessedSample process(const EcgSample& sample);

private:
    ExponentialDcBlocker dc_blocker_;
    ExponentialLowPass low_pass_;
};

}

