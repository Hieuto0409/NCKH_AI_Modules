#pragma once

#include "types/feature_packet.h"

namespace ppgfw {

FusionFeatureSet buildFusionFeatures(const MetricSnapshot& metrics, uint64_t last_ecg_peak_us,
                                     uint64_t last_ppg_peak_us);

}

