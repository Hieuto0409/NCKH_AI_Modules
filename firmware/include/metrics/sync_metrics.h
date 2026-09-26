#pragma once

#include "types/feature_packet.h"

namespace ppgfw {

class SyncMetrics {
public:
    static FusionFeatureSet calculate(const MetricSnapshot& metrics, uint64_t last_ecg_peak_us,
                                      uint64_t last_ppg_peak_us);
};

}

