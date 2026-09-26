#include "dsp/rpeak_detector.h"

#include "config/thresholds.h"

#include <cmath>

namespace ppgfw {

void RPeakDetector::reset() {
    previous_two_ = 0.0F;
    previous_one_ = 0.0F;
    previous_one_timestamp_us_ = 0;
    last_peak_timestamp_us_ = 0;
    envelope_ = 1.0F;
    samples_seen_ = 0;
}

PeakEvent RPeakDetector::update(float value, uint64_t timestamp_us) {
    const float magnitude = std::fabs(value);
    envelope_ = 0.995F * envelope_ + 0.005F * magnitude;
    PeakEvent event{};
    if (samples_seen_ >= 2) {
        const uint64_t refractory_us = config::candidate::kEcgPeakRefractoryMs * 1000ULL;
        const bool refractory_ok = last_peak_timestamp_us_ == 0 ||
                                   previous_one_timestamp_us_ - last_peak_timestamp_us_ >= refractory_us;
        const bool local_maximum = previous_one_ > previous_two_ && previous_one_ >= magnitude;
        const bool above_threshold = previous_one_ > envelope_ * config::candidate::kEcgPeakThresholdScale;
        if (refractory_ok && local_maximum && above_threshold) {
            event = {true, previous_one_timestamp_us_, previous_one_};
            last_peak_timestamp_us_ = previous_one_timestamp_us_;
        }
    }
    previous_two_ = previous_one_;
    previous_one_ = magnitude;
    previous_one_timestamp_us_ = timestamp_us;
    if (samples_seen_ < 3) {
        ++samples_seen_;
    }
    return event;
}

}

