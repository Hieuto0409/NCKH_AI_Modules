#include "dsp/filters.h"

namespace ppgfw {

ExponentialDcBlocker::ExponentialDcBlocker(float alpha) : alpha_(alpha) {
}

void ExponentialDcBlocker::reset() {
    dc_ = 0.0F;
    initialized_ = false;
}

float ExponentialDcBlocker::update(float input) {
    if (!initialized_) {
        dc_ = input;
        initialized_ = true;
    } else {
        dc_ += alpha_ * (input - dc_);
    }
    return input - dc_;
}

float ExponentialDcBlocker::dc() const {
    return dc_;
}

ExponentialLowPass::ExponentialLowPass(float alpha) : alpha_(alpha) {
}

void ExponentialLowPass::reset() {
    state_ = 0.0F;
    initialized_ = false;
}

float ExponentialLowPass::update(float input) {
    if (!initialized_) {
        state_ = input;
        initialized_ = true;
    } else {
        state_ += alpha_ * (input - state_);
    }
    return state_;
}

}

