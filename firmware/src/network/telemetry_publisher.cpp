#include "network/telemetry_publisher.h"

#include "config/versions.h"

#include <cmath>
#include <cstdio>

namespace ppgfw {

namespace {

void jsonNumber(char* output, size_t size, const MetricValue& metric) {
    if (metric.status == ValueStatus::Valid && std::isfinite(metric.value)) {
        std::snprintf(output, size, "%.3f", metric.value);
    } else {
        std::snprintf(output, size, "null");
    }
}

}

TelemetryPublisher::TelemetryPublisher(MqttClient& client) : client_(client) {
}

void TelemetryPublisher::publish(const ResultSnapshot& result) {
    char ecg_hr[20]{};
    char ppg_hr[20]{};
    char spo2[20]{};
    jsonNumber(ecg_hr, sizeof(ecg_hr), result.feature_packet.metrics.ecg_heart_rate_bpm);
    jsonNumber(ppg_hr, sizeof(ppg_hr), result.feature_packet.metrics.ppg_pulse_rate_bpm);
    const Spo2Result& oxygen = result.feature_packet.metrics.spo2;
    if (oxygen.status == ValueStatus::Valid && std::isfinite(oxygen.percent)) {
        std::snprintf(spo2, sizeof(spo2), "%.3f", oxygen.percent);
    } else {
        std::snprintf(spo2, sizeof(spo2), "null");
    }
    char payload[1536]{};
    const int length = std::snprintf(payload, sizeof(payload),
                  "{\"schema\":%u,\"timestamp_us\":%llu,\"ecg_hr\":%s,\"ppg_hr\":%s,"
                  "\"spo2\":%s,\"ppg_sqi\":%.3f,\"ecg_sqi\":%.3f,\"normal\":%s,"
                  "\"remeasure\":%s,\"stress_status\":%u,\"low_o2_status\":%u,"
                  "\"rhythm_status\":%u,\"stress_model\":%u,\"low_o2_model\":%u,"
                  "\"rhythm_model\":%u,\"stress_scaler\":%u,\"low_o2_scaler\":%u,"
                  "\"rhythm_scaler\":%u,\"stress_latency_us\":%lu,"
                  "\"rhythm_latency_us\":%lu,\"rhythm_label\":%u,"
                  "\"spo2_clinically_validated\":%s,\"ai_commit\":\"%s\"}",
                  static_cast<unsigned>(result.feature_packet.schema),
                  static_cast<unsigned long long>(result.timestamp_us), ecg_hr, ppg_hr, spo2,
                  result.feature_packet.ppg_quality.score,
                  result.feature_packet.ecg_quality.score,
                  result.normal ? "true" : "false", result.remeasure ? "true" : "false",
                  static_cast<unsigned>(result.stress.status),
                  static_cast<unsigned>(result.low_o2.status),
                  static_cast<unsigned>(result.rhythm.status),
                  static_cast<unsigned>(result.stress.model_version),
                  static_cast<unsigned>(result.low_o2.model_version),
                  static_cast<unsigned>(result.rhythm.model_version),
                  static_cast<unsigned>(result.stress.scaler_version),
                  static_cast<unsigned>(result.low_o2.scaler_version),
                  static_cast<unsigned>(result.rhythm.scaler_version),
                  static_cast<unsigned long>(result.stress.inference_latency_us),
                  static_cast<unsigned long>(result.rhythm.inference_latency_us),
                  static_cast<unsigned>(result.rhythm.label),
                  result.feature_packet.metrics.spo2.clinically_validated ? "true" : "false",
                  config::kAiRepositoryCommit);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(payload)) return;
    const auto& p = result.feature_packet;
    char stress_score[20], rhythm_score[20];
    jsonNumber(stress_score,sizeof(stress_score),{result.stress.status==InferenceStatus::Valid ? ValueStatus::Valid : ValueStatus::NotAvailable,result.stress.score});
    jsonNumber(rhythm_score,sizeof(rhythm_score),{result.rhythm.status==InferenceStatus::Valid ? ValueStatus::Valid : ValueStatus::NotAvailable,result.rhythm.score});
    const int extra = std::snprintf(payload+length-1,sizeof(payload)-length+1,
        ",\"stress_label\":%u,\"stress_probability\":%s,\"af_probability\":%s,"
        "\"stress_reason\":%lu,\"rhythm_reason\":%lu,\"low_o2_reason\":%lu,"
        "\"ppg_window_us\":[%llu,%llu],\"ecg_window_us\":[%llu,%llu],\"spo2_window_us\":[%llu,%llu],"
        "\"ppg_quality_reason\":%lu,\"ecg_quality_reason\":%lu,\"stress_intervals\":%u,\"ecg_intervals\":%u}",
        unsigned(result.stress.label),stress_score,rhythm_score,
        static_cast<unsigned long>(result.stress.reason_flags),static_cast<unsigned long>(result.rhythm.reason_flags),
        static_cast<unsigned long>(result.low_o2.reason_flags),
        static_cast<unsigned long long>(p.ppg_window.start_timestamp_us),static_cast<unsigned long long>(p.ppg_window.end_timestamp_us),
        static_cast<unsigned long long>(p.ecg_window.start_timestamp_us),static_cast<unsigned long long>(p.ecg_window.end_timestamp_us),
        static_cast<unsigned long long>(p.spo2_window.start_timestamp_us),static_cast<unsigned long long>(p.spo2_window.end_timestamp_us),
        static_cast<unsigned long>(p.ppg_quality.reasons),static_cast<unsigned long>(p.ecg_quality.reasons),
        unsigned(p.stress_ppg.clean_interval_count),unsigned(p.ecg_af.interval_count));
    if (extra < 0 || static_cast<size_t>(extra) >= sizeof(payload)-length+1) return;
    client_.publish(payload);
}

}
