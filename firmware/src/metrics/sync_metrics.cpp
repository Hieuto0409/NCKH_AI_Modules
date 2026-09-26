#include "metrics/sync_metrics.h"

#include <cmath>

namespace ppgfw {

FusionFeatureSet SyncMetrics::calculate(const MetricSnapshot& metrics, uint64_t last_ecg_peak_us,
                                        uint64_t last_ppg_peak_us) {
    FusionFeatureSet output{};
    if (metrics.ecg_heart_rate_bpm.status == ValueStatus::Valid &&
        metrics.ppg_pulse_rate_bpm.status == ValueStatus::Valid) {
        output.heart_rate_difference_bpm = std::fabs(metrics.ecg_heart_rate_bpm.value -
                                                     metrics.ppg_pulse_rate_bpm.value);
    }
    if (last_ppg_peak_us > last_ecg_peak_us && last_ecg_peak_us > 0) {
        const float delay_ms = static_cast<float>(last_ppg_peak_us - last_ecg_peak_us) / 1000.0F;
        if (delay_ms >= 50.0F && delay_ms <= 600.0F) {
            output.pulse_arrival_candidate_ms = delay_ms;
            output.pulse_arrival_valid = true;
            output.matched_beat_count = 1;
        }
    }
    return output;
}

}

