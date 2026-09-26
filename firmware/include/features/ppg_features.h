#pragma once

#include "dsp/ac_dc_estimator.h"
#include "types/feature_packet.h"

namespace ppgfw {

PpgFeatureSet buildPpgFeatures(const AcDcSnapshot& signal, const MetricSnapshot& metrics,
                               const QualitySummary& quality, uint16_t peak_count,
                               float mean_peak_amplitude, float peak_amplitude_variation);

}

