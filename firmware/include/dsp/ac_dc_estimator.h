#pragma once

#include "utils/running_statistics.h"

namespace ppgfw {

struct AcDcSnapshot {
    SignalStatistics red{};
    SignalStatistics ir{};
    float red_ac{};
    float red_dc{};
    float ir_ac{};
    float ir_dc{};
    float correlation{};
};

class AcDcEstimator {
public:
    void reset();
    void add(uint32_t red, uint32_t ir);
    AcDcSnapshot snapshot() const;

private:
    RunningStatistics red_{};
    RunningStatistics ir_{};
    RunningCorrelation correlation_{};
};

}

