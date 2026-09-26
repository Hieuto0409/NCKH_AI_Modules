#include "features/ecg_features.h"

#include <cmath>

namespace ppgfw {

EcgFeatureSet buildEcgFeatures(const SignalStatistics& signal, const MetricSnapshot& metrics,
                               uint16_t peak_count, float mean_peak_amplitude) {
    EcgFeatureSet output{};
    output.mean = static_cast<float>(signal.mean);
    output.standard_deviation = static_cast<float>(std::sqrt(signal.variance));
    output.signal_range = static_cast<float>(signal.maximum - signal.minimum);
    output.heart_rate_bpm = metrics.ecg_heart_rate_bpm.value;
    output.mean_rr_ms = metrics.hrv.mean_interval_ms;
    output.sdnn_ms = metrics.hrv.sdnn_ms;
    output.rmssd_ms = metrics.hrv.rmssd_ms;
    output.pnn50_percent = metrics.hrv.pnn50_percent;
    output.mean_rpeak_amplitude = mean_peak_amplitude;
    output.rpeak_count = peak_count;
    output.valid_feature_count = metrics.hrv.status == ValueStatus::Valid ? 10 : 4;
    return output;
}

}

