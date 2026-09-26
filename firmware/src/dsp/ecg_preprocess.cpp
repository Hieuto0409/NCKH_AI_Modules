#include "dsp/ecg_preprocess.h"

#include "config/thresholds.h"

namespace ppgfw {

EcgPreprocessor::EcgPreprocessor()
    : dc_blocker_(config::candidate::kEcgDcAlpha),
      low_pass_(config::candidate::kEcgLowPassAlpha) {
}

void EcgPreprocessor::reset() {
    dc_blocker_.reset();
    low_pass_.reset();
}

EcgProcessedSample EcgPreprocessor::process(const EcgSample& sample) {
    const float value = low_pass_.update(dc_blocker_.update(static_cast<float>(sample.raw)));
    return {sample.timestamp_us, value, dc_blocker_.dc(), sample.flags};
}

}

