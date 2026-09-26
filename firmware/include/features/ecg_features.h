#pragma once

#include "types/feature_packet.h"

namespace ppgfw {

EcgFeatureSet buildEcgFeatures(const SignalStatistics& signal, const MetricSnapshot& metrics,
                               uint16_t peak_count, float mean_peak_amplitude);

}

