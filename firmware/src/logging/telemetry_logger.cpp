#include "logging/telemetry_logger.h"

#include "config/board_config.h"

#include <Arduino.h>
#include <cmath>

namespace ppgfw {

namespace {

const char* inferenceText(InferenceStatus status) {
    switch (status) {
        case InferenceStatus::Valid: return "valid";
        case InferenceStatus::InvalidInput: return "invalid_input";
        case InferenceStatus::NotReady: return "not_ready";
        case InferenceStatus::QualityRejected: return "quality_rejected";
        case InferenceStatus::Error: return "error";
    }
    return "unknown";
}

}

void TelemetryLogger::logDiagnostics(const IntegrityDiagnostics& ppg,
                                     const IntegrityDiagnostics& ecg) {
#if !APP_ENABLE_BINARY_LOG && !APP_ENABLE_CSV_LOG
    if (Serial.availableForWrite() < 96) {
        return;
    }
    Serial.printf("DIAG ppg=%lu/%lu drop=%lu ovf=%lu q=%lu ecg=%lu/%lu drop=%lu lead=%lu q=%lu\n",
                  static_cast<unsigned long>(ppg.received_samples),
                  static_cast<unsigned long>(ppg.expected_samples),
                  static_cast<unsigned long>(ppg.dropped_samples),
                  static_cast<unsigned long>(ppg.overflow_count),
                  static_cast<unsigned long>(ppg.queue_drop_count),
                  static_cast<unsigned long>(ecg.received_samples),
                  static_cast<unsigned long>(ecg.expected_samples),
                  static_cast<unsigned long>(ecg.dropped_samples),
                  static_cast<unsigned long>(ecg.lead_off_count),
                  static_cast<unsigned long>(ecg.queue_drop_count));
#else
    (void)ppg;
    (void)ecg;
#endif
}

void TelemetryLogger::logResult(const ResultSnapshot& result) {
#if !APP_ENABLE_BINARY_LOG && !APP_ENABLE_CSV_LOG
    const MetricSnapshot& metrics = result.feature_packet.metrics;
    Serial.printf("RESULT ppg_sqi=%.3f ecg_sqi=%.3f HR=",
                  result.feature_packet.ppg_quality.score,
                  result.feature_packet.ecg_quality.score);
    if (metrics.ecg_heart_rate_bpm.status == ValueStatus::Valid) {
        Serial.printf("%.1f", metrics.ecg_heart_rate_bpm.value);
    } else {
        Serial.print("N/A");
    }
    Serial.print(" pulse=");
    if (metrics.ppg_pulse_rate_bpm.status == ValueStatus::Valid) {
        Serial.printf("%.1f", metrics.ppg_pulse_rate_bpm.value);
    } else {
        Serial.print("N/A");
    }
    Serial.printf(" SpO2=%s stress=%s low_o2=%s rhythm=%s normal=%u remeasure=%u\n",
                  metrics.spo2.status == ValueStatus::Valid ? "valid" : "N/A-unvalidated",
                  inferenceText(result.stress.status), inferenceText(result.low_o2.status),
                  inferenceText(result.rhythm.status), result.normal ? 1U : 0U,
                  result.remeasure ? 1U : 0U);
    const auto& p=result.feature_packet;
    Serial.printf("WINDOW ppg=[%llu,%llu) ecg=[%llu,%llu) spo2=[%llu,%llu) reason=%lu/%lu intervals=%u/%u\n",
        p.ppg_window.start_timestamp_us,p.ppg_window.end_timestamp_us,
        p.ecg_window.start_timestamp_us,p.ecg_window.end_timestamp_us,
        p.spo2_window.start_timestamp_us,p.spo2_window.end_timestamp_us,
        static_cast<unsigned long>(p.ppg_quality.reasons),static_cast<unsigned long>(p.ecg_quality.reasons),
        unsigned(p.stress_ppg.clean_interval_count),unsigned(p.ecg_af.interval_count));
    Serial.printf("AI stress_label=%u score=%.6f reason=%lu rhythm_label=%u score=%.6f reason=%lu\n",
        unsigned(result.stress.label),result.stress.score,static_cast<unsigned long>(result.stress.reason_flags),
        unsigned(result.rhythm.label),result.rhythm.score,static_cast<unsigned long>(result.rhythm.reason_flags));
#else
    (void)result;
#endif
}

}
