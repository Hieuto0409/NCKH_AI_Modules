#pragma once

#include "config/sampling.h"
#include "dsp/ac_dc_estimator.h"
#include "dsp/ecg_preprocess.h"
#include "dsp/peak_detector_ppg.h"
#include "dsp/ppg_preprocess.h"
#include "dsp/rpeak_detector.h"
#include "features/ecg_af_feature_adapter.h"
#include "features/ecg_window_builder.h"
#include "features/stress_ppg_60s_adapter.h"
#include "metrics/spo2_rate_adapter.h"
#include "types/feature_packet.h"
#include "types/raw_samples.h"
#include "types/runtime_types.h"
#include "utils/running_statistics.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ppgfw {

class FeatureBuilder {
public:
    void reset(uint64_t window_start_us);
    void resetPipeline();
    void beginMeasurementWindow(uint64_t start_us);
    bool windowDrained(uint64_t now_us) const;
    void consume(const PpgSample& sample);
    void consume(const EcgSample& sample);
    FeaturePacket build(uint64_t window_end_us, const IntegrityDiagnostics& ppg_integrity,
                        const IntegrityDiagnostics& ecg_integrity);

private:

    uint64_t window_start_us_{};
    uint64_t window_end_us_{};
    uint64_t ppg_watermark_{}, ecg_watermark_{};
    bool measuring_{}, have_ppg_{}, have_ecg_{};
    uint32_t last_ppg_sequence_{}, last_ecg_sequence_{};
    uint64_t last_ppg_timestamp_{}, last_ecg_timestamp_{};
    IntegrityDiagnostics ppg_integrity_{};
    EcgWindowBuilder ecg_windows_{};
    PpgPreprocessor ppg_preprocessor_{};
    EcgPreprocessor ecg_preprocessor_{};
    PpgPeakDetector ppg_peak_detector_{};
    RPeakDetector rpeak_detector_{};
    StressPpg60sAdapter stress_adapter_{};
    Spo2RateAdapter200To100 spo2_adapter_{};
    AcDcEstimator ac_dc_estimator_{};
    RunningStatistics ppg_peak_amplitudes_{};
    uint16_t ppg_peak_count_{};
    uint64_t last_spo2_source_us_{};
    uint32_t ppg_contact_samples_{};
};

}
