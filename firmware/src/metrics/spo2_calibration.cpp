#include "metrics/spo2_calibration.h"

#include "config/thresholds.h"

namespace ppgfw {

bool Spo2Calibration::validated() {
    return config::candidate::kSpo2CalibrationValidated;
}

float Spo2Calibration::apply(float uncalibrated_percent) {
    return uncalibrated_percent;
}

}

