#pragma once

namespace ppgfw {

class Spo2Calibration {
public:
    static bool validated();
    static float apply(float uncalibrated_percent);
};

}

