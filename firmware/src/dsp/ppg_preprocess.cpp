#include "dsp/ppg_preprocess.h"

#include "config/thresholds.h"

namespace ppgfw {

PpgPreprocessor::PpgPreprocessor()
    : red_dc_blocker_(config::candidate::kPpgDcAlpha),
      ir_dc_blocker_(config::candidate::kPpgDcAlpha),
      red_low_pass_(config::candidate::kPpgLowPassAlpha),
      ir_low_pass_(config::candidate::kPpgLowPassAlpha) {
}

void PpgPreprocessor::reset() {
    red_dc_blocker_.reset();
    ir_dc_blocker_.reset();
    red_low_pass_.reset();
    ir_low_pass_.reset();
}

PpgProcessedSample PpgPreprocessor::process(const PpgSample& sample) {
    const float red = red_low_pass_.update(red_dc_blocker_.update(static_cast<float>(sample.red)));
    const float ir = ir_low_pass_.update(ir_dc_blocker_.update(static_cast<float>(sample.ir)));
    return {sample.timestamp_us, red, ir, red_dc_blocker_.dc(), ir_dc_blocker_.dc(), sample.flags};
}

}

