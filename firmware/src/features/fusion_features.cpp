#include "features/fusion_features.h"

#include "metrics/sync_metrics.h"

namespace ppgfw {

FusionFeatureSet buildFusionFeatures(const MetricSnapshot& metrics, uint64_t last_ecg_peak_us,
                                     uint64_t last_ppg_peak_us) {
    return SyncMetrics::calculate(metrics, last_ecg_peak_us, last_ppg_peak_us);
}

}

