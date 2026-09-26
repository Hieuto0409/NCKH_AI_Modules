#include "dsp/ac_dc_estimator.h"

#include <cmath>

namespace ppgfw {

void AcDcEstimator::reset() {
    red_.reset();
    ir_.reset();
    correlation_.reset();
}

void AcDcEstimator::add(uint32_t red, uint32_t ir) {
    red_.add(red);
    ir_.add(ir);
    correlation_.add(red, ir);
}

AcDcSnapshot AcDcEstimator::snapshot() const {
    AcDcSnapshot value{};
    value.red = red_.snapshot();
    value.ir = ir_.snapshot();
    value.red_ac = static_cast<float>(std::sqrt(value.red.variance));
    value.red_dc = static_cast<float>(value.red.mean);
    value.ir_ac = static_cast<float>(std::sqrt(value.ir.variance));
    value.ir_dc = static_cast<float>(value.ir.mean);
    value.correlation = correlation_.value();
    return value;
}

}

