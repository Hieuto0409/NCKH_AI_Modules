#include "app/measurement_state_machine.h"

#include "config/sampling.h"

namespace ppgfw {

void MeasurementStateMachine::begin(uint64_t now_us) {
    transition(MeasurementState::SelfTest, now_us);
}

void MeasurementStateMachine::selfTestComplete(bool success, uint64_t now_us) {
    if (state_ != MeasurementState::SelfTest) {
        return;
    }
    transition(success ? MeasurementState::Idle : MeasurementState::Error, now_us);
}

void MeasurementStateMachine::update(uint64_t now_us, bool start_pressed,
                                     bool cancel_pressed, bool contact,
                                     bool lead_off, bool window_drained) {
    switch (state_) {
        case MeasurementState::Idle:
            if (start_pressed) {
                transition(MeasurementState::ContactWait, now_us);
            }
            break;
        case MeasurementState::ContactWait:
            if (cancel_pressed) {
                transition(MeasurementState::Idle, now_us);
                break;
            }
            if (contact && !lead_off) {
                if (stable_contact_at_us_ == 0) {
                    stable_contact_at_us_ = now_us;
                } else if (now_us - stable_contact_at_us_ >= config::kContactStableMs * 1000ULL) {
                    transition(MeasurementState::Warmup, now_us);
                }
            } else {
                stable_contact_at_us_ = 0;
            }
            break;
        case MeasurementState::Warmup:
            if (cancel_pressed) {
                transition(MeasurementState::Idle, now_us);
            } else if (!contact || lead_off) {
                transition(MeasurementState::ContactWait, now_us);
            } else if (now_us - entered_at_us_ >= config::kWarmupMs * 1000ULL) {
                transition(MeasurementState::Measuring, now_us);
            }
            break;
        case MeasurementState::Measuring:
            if (cancel_pressed) {
                transition(MeasurementState::Idle, now_us);
            } else if (now_us >= measurementEndUs() && window_drained) {
                transition(MeasurementState::QualityEvaluation, now_us);
            }
            break;
        case MeasurementState::Result:
            if (start_pressed) {
                transition(MeasurementState::ContactWait, now_us);
            } else if (cancel_pressed || now_us - entered_at_us_ >= config::kResultHoldMs * 1000ULL) {
                transition(MeasurementState::Idle, now_us);
            }
            break;
        case MeasurementState::Error:
            if (start_pressed) {
                transition(MeasurementState::SelfTest, now_us);
            }
            break;
        case MeasurementState::Boot:
        case MeasurementState::SelfTest:
        case MeasurementState::QualityEvaluation:
            break;
    }
}

void MeasurementStateMachine::qualityEvaluated(uint64_t now_us) {
    if (state_ == MeasurementState::QualityEvaluation) {
        transition(MeasurementState::Result, now_us);
    }
}

uint64_t MeasurementStateMachine::measurementEndUs() const {
    return measurement_start_us_ + config::kMeasurementWindowMs * 1000ULL;
}

MeasurementState MeasurementStateMachine::state() const {
    return state_;
}

uint32_t MeasurementStateMachine::enteredAtMs() const {
    return static_cast<uint32_t>(entered_at_us_ / 1000);
}

uint32_t MeasurementStateMachine::remainingMs(uint64_t now_us) const {
    uint32_t duration = 0;
    if (state_ == MeasurementState::Warmup) {
        duration = config::kWarmupMs;
    } else if (state_ == MeasurementState::Measuring) {
        duration = config::kMeasurementWindowMs;
    } else if (state_ == MeasurementState::Result) {
        duration = config::kResultHoldMs;
    }
    const uint64_t elapsed = (now_us - entered_at_us_) / 1000;
    return elapsed >= duration ? 0 : duration - elapsed;
}

void MeasurementStateMachine::transition(MeasurementState next, uint64_t now_us) {
    state_ = next;
    if (next == MeasurementState::Measuring) measurement_start_us_ = now_us;
    entered_at_us_ = now_us;
    stable_contact_at_us_ = 0;
}

}
