#include "app/app_controller.h"

#include "acquisition/timestamp_service.h"
#include "config/board_config.h"
#include "config/sampling.h"
#include "config/versions.h"

#include <Arduino.h>
#include <algorithm>
#include <esp_sleep.h>
#include <driver/gpio.h>

namespace ppgfw {

void AppController::begin() {
    Serial.begin(config::kSerialBaud);
    start_button_.begin();
    cancel_button_.begin();
    oled_.begin();
    light_sleep_ready_ = gpio_wakeup_enable(static_cast<gpio_num_t>(config::pins::kButton1), GPIO_INTR_LOW_LEVEL) == ESP_OK &&
        gpio_wakeup_enable(static_cast<gpio_num_t>(config::pins::kButton2), GPIO_INTR_LOW_LEVEL) == ESP_OK &&
        esp_sleep_enable_gpio_wakeup() == ESP_OK;
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
    // FIFO retains samples between polls; avoid busy I2C transactions on every loop.
    if (acquisition_active_ && millis() - last_ppg_poll_ms_ >= 10) {
        last_ppg_poll_ms_ = millis();
        ppg_acquisition_.poll();
    }

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
        setAcquisitionActive(false);
        finishMeasurement(state_machine_.measurementEndUs(), now_ms);
        current = state_machine_.state();
    }

    if (state_before != current && current == MeasurementState::Warmup) feature_builder_.resetPipeline();

    const bool acquiring = current == MeasurementState::ContactWait ||
                           current == MeasurementState::Warmup || current == MeasurementState::Measuring;
    if (acquiring) mqtt_client_.suspend(); // shut down radio before waking sensors
    if (!setAcquisitionActive(acquiring)) {
        state_machine_.acquisitionFailed(TimestampService::nowUs());
        current = state_machine_.state();
        setAcquisitionActive(false);
    }
    binary_logger_.flush(8);
    mqtt_client_.loop(millis(), !acquisition_active_ &&
        (current == MeasurementState::Idle || current == MeasurementState::Result));
#if APP_ENABLE_MQTT && !APP_ENABLE_BINARY_LOG && !APP_ENABLE_CSV_LOG
    if (!acquisition_active_ && (last_network_pending_ != mqtt_client_.pending() ||
        last_network_acked_ != mqtt_client_.acknowledged() || last_network_rejected_ != mqtt_client_.rejected())) {
        last_network_pending_ = mqtt_client_.pending();
        last_network_acked_ = mqtt_client_.acknowledged();
        last_network_rejected_ = mqtt_client_.rejected();
        Serial.printf("UPLOAD pending=%u broker_acked=%lu rejected=%lu\n",
            unsigned(last_network_pending_), static_cast<unsigned long>(last_network_acked_),
            static_cast<unsigned long>(last_network_rejected_));
    }
#endif
    updatePresentation(millis());
    previous_state_ = current;
    if (light_sleep_ready_ && !acquisition_active_ && !mqtt_client_.radioActive() &&
        current == MeasurementState::Idle &&
        static_cast<uint32_t>(millis() - state_machine_.enteredAtMs()) >= 15000 &&
        digitalRead(config::pins::kButton1) == HIGH && digitalRead(config::pins::kButton2) == HIGH) {
        Serial.flush();
        if (esp_sleep_enable_timer_wakeup(1000000) == ESP_OK) esp_light_sleep_start();
        // GPIO 47/48 can wake light sleep; do not use RTC-only deep-sleep wake.
    } else if (!acquisition_active_) delay(10); // yield while waiting for buttons/network
    else delay(1);
}

bool AppController::setAcquisitionActive(bool active) {
#if APP_ENABLE_BINARY_LOG || APP_ENABLE_CSV_LOG
    active = true; // explicit capture profiles retain continuous acquisition
#endif
    if (active == acquisition_active_) return true;
    const bool ppg_ok = ppg_acquisition_.setActive(active);
    const bool ecg_ok = ecg_acquisition_.setActive(active);
    acquisition_active_ = active;
    return !active || (ppg_ok && ecg_ok);
}

bool AppController::runSelfTest(uint32_t now_ms) {
    const bool ppg_ok = ppg_acquisition_.begin();
    const bool ecg_ok = ecg_acquisition_.begin();
    const bool success = ppg_ok && ecg_ok;
    acquisition_active_ = true;
    setAcquisitionActive(false);
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
    const auto state = state_machine_.state();
    const bool quiet = state == MeasurementState::Warmup || state == MeasurementState::Measuring;
    const bool idle_sleep = state == MeasurementState::Idle &&
        static_cast<uint32_t>(now_ms - state_machine_.enteredAtMs()) >= 15000;
    oled_.setSleeping(quiet || idle_sleep);
    if (!quiet && !idle_sleep && (state != previous_state_ ||
        now_ms - last_oled_update_ms_ >= config::kOledRefreshMs)) {
        last_oled_update_ms_ = now_ms;
        oled_.render(state_machine_.state(), result_, state_machine_.remainingMs(TimestampService::nowUs()),
                     ppg_acquisition_.sensorAvailable(), ppg_acquisition_.contactDetected(),
                     ecg_acquisition_.leadOff());
    }
    if (!quiet && now_ms - last_telemetry_ms_ >= config::kTelemetryPeriodMs) {
        last_telemetry_ms_ = now_ms;
        telemetry_logger_.logDiagnostics(ppg_acquisition_.diagnostics(),
                                         ecg_acquisition_.diagnostics());
    }
}

}
