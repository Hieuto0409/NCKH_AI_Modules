#include "app/app_controller.h"

#include "acquisition/timestamp_service.h"
#include "config/board_config.h"
#include "config/sampling.h"
#include "config/versions.h"

#include <Arduino.h>
#include <algorithm>

namespace ppgfw {

void AppController::begin() {
    Serial.begin(config::kSerialBaud);
    start_button_.begin();
    cancel_button_.begin();
    oled_.begin();
    csv_logger_.begin();
    mqtt_client_.begin();
    binary_logger_.begin(TimestampService::nowUs(), ESP.getEfuseMac());

    const uint32_t now_ms = millis();
    state_machine_.begin(TimestampService::nowUs());
    previous_state_ = state_machine_.state();
    runSelfTest(now_ms);

#if !APP_ENABLE_BINARY_LOG && !APP_ENABLE_CSV_LOG
    Serial.printf("PPGFW %s | MAX30102=%luHz ECG=%luHz | PSRAM=%s | AI=%s\n",
                  config::kFirmwareVersion,
                  static_cast<unsigned long>(config::kPpgSampleRateHz),
                  static_cast<unsigned long>(config::kEcgSampleRateHz),
                  psramFound() ? "yes" : "no", config::kAiRepositoryCommit);
#endif
}

void AppController::loop() {
    ppg_acquisition_.poll();

    const uint32_t now_ms = millis();
    start_button_.update(now_ms);
    cancel_button_.update(now_ms);

    const MeasurementState state_before = state_machine_.state();
    consumeSamples(state_before == MeasurementState::Measuring || state_before == MeasurementState::Warmup);
    const uint64_t now_us = TimestampService::nowUs();
    state_machine_.update(now_us, start_button_.pressed(), cancel_button_.pressed(),
                          ppg_acquisition_.contactDetected(), ecg_acquisition_.leadOff(), feature_builder_.windowDrained(now_us));

    MeasurementState current = state_machine_.state();
    if (state_before != current && current == MeasurementState::SelfTest) {
        runSelfTest(now_ms);
        current = state_machine_.state();
    }
    if (state_before != current && current == MeasurementState::Measuring) {
        startMeasurement(state_machine_.measurementStartUs());
    } else if (state_before == MeasurementState::Measuring &&
               current == MeasurementState::QualityEvaluation) {
        finishMeasurement(state_machine_.measurementEndUs(), now_ms);
        current = state_machine_.state();
    }

    if (state_before != current && current == MeasurementState::Warmup) feature_builder_.resetPipeline();

    binary_logger_.flush(8);
    if (current == MeasurementState::Idle || current == MeasurementState::Result ||
        current == MeasurementState::Error) {
        mqtt_client_.loop(now_ms);
    }
    updatePresentation(now_ms);
    previous_state_ = current;
}

bool AppController::runSelfTest(uint32_t now_ms) {
    const bool ppg_ok = ppg_acquisition_.begin();
    const bool ecg_ok = ecg_acquisition_.begin();
    const bool success = ppg_ok && ecg_ok;
    state_machine_.selfTestComplete(success, TimestampService::nowUs());
#if !APP_ENABLE_BINARY_LOG && !APP_ENABLE_CSV_LOG
    Serial.printf("SELFTEST ppg=%s ecg=%s backend=%s\n", ppg_ok ? "ok" : "fail",
                  ecg_ok ? "ok" : "fail", ecg_backend_.name());
#endif
    return success;
}

void AppController::consumeSamples(bool include_in_measurement) {
    PpgSample ppg_sample{};
    while (ppg_queue_.pop(ppg_sample)) {
        if (include_in_measurement) {
            feature_builder_.consume(ppg_sample);
        }
        binary_logger_.enqueue(ppg_sample);
        csv_logger_.log(ppg_sample);
    }
    EcgSample ecg_sample{};
    while (ecg_queue_.pop(ecg_sample)) {
        if (include_in_measurement) {
            feature_builder_.consume(ecg_sample);
        }
        binary_logger_.enqueue(ecg_sample);
        csv_logger_.log(ecg_sample);
    }
}

void AppController::startMeasurement(uint64_t now_us) {
    ppg_acquisition_.resetWindowDiagnostics();
    ecg_acquisition_.resetWindowDiagnostics();
    feature_builder_.beginMeasurementWindow(now_us);
    result_ = {};
}

void AppController::finishMeasurement(uint64_t now_us, uint32_t now_ms) {
    const FeaturePacket packet = feature_builder_.build(now_us, {}, {});
#if APP_ENABLE_AI
    const BranchResult stress = stress_engine_.infer(packet);
    const BranchResult low_o2 = low_o2_engine_.infer(packet);
    const BranchResult rhythm = rhythm_engine_.infer(packet);
#else
    const BranchResult stress{};
    const BranchResult low_o2{};
    const BranchResult rhythm{};
#endif
    result_ = DecisionAggregator::aggregate(now_us, packet, stress, low_o2, rhythm);
    binary_logger_.enqueue(result_);
    telemetry_logger_.logResult(result_);
    telemetry_publisher_.publish(result_);
    state_machine_.qualityEvaluated(TimestampService::nowUs());
}

void AppController::updatePresentation(uint32_t now_ms) {
    if (now_ms - last_oled_update_ms_ >= config::kOledRefreshMs) {
        last_oled_update_ms_ = now_ms;
        oled_.render(state_machine_.state(), result_, state_machine_.remainingMs(TimestampService::nowUs()),
                     ppg_acquisition_.sensorAvailable(), ppg_acquisition_.contactDetected(),
                     ecg_acquisition_.leadOff());
    }
    if (now_ms - last_telemetry_ms_ >= config::kTelemetryPeriodMs) {
        last_telemetry_ms_ = now_ms;
        telemetry_logger_.logDiagnostics(ppg_acquisition_.diagnostics(),
                                         ecg_acquisition_.diagnostics());
    }
}

}
