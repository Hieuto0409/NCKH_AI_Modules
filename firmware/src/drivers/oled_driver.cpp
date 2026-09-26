#include "drivers/oled_driver.h"

#include "app/measurement_state_machine.h"
#include "config/pins.h"

#include <cmath>
#include <cstdio>

namespace ppgfw {

namespace {

const char* stateText(MeasurementState state) {
    switch (state) {
        case MeasurementState::Boot: return "BOOT";
        case MeasurementState::SelfTest: return "SELF TEST";
        case MeasurementState::Idle: return "READY";
        case MeasurementState::ContactWait: return "PLACE FINGER";
        case MeasurementState::Warmup: return "STABILIZING";
        case MeasurementState::Measuring: return "MEASURING";
        case MeasurementState::QualityEvaluation: return "CHECKING SQI";
        case MeasurementState::Result: return "RESULT";
        case MeasurementState::Error: return "SENSOR ERROR";
    }
    return "UNKNOWN";
}

void metricText(char* output, size_t size, const char* label, const MetricValue& metric) {
    if (metric.status == ValueStatus::Valid && std::isfinite(metric.value)) {
        std::snprintf(output, size, "%s %.1f", label, metric.value);
    } else {
        std::snprintf(output, size, "%s N/A", label);
    }
}

}

OledDriver::OledDriver()
#if APP_ENABLE_OLED
    : display_(U8G2_R0, config::pins::kOledSclk, config::pins::kOledMosi,
               config::pins::kOledCs, config::pins::kOledDc, config::pins::kOledReset)
#endif
{
}

bool OledDriver::begin() {
#if APP_ENABLE_OLED
    available_ = display_.begin();
    if (available_) {
        display_.setFont(u8g2_font_6x10_tf);
    }
#else
    available_ = false;
#endif
    return available_;
}

void OledDriver::setSleeping(bool sleeping) {
#if APP_ENABLE_OLED
    if (available_ && sleeping != sleeping_) display_.setPowerSave(sleeping ? 1 : 0);
#endif
    sleeping_ = sleeping;
}

void OledDriver::render(MeasurementState state, const ResultSnapshot& result,
                        uint32_t remaining_ms, bool ppg_available, bool contact,
                        bool lead_off) {
#if APP_ENABLE_OLED
    if (!available_) {
        return;
    }
    char line[24]{};
    display_.clearBuffer();
    display_.setFont(u8g2_font_6x10_tf);
    display_.drawStr(0, 10, stateText(state));
    if (state == MeasurementState::Warmup || state == MeasurementState::Measuring) {
        std::snprintf(line, sizeof(line), "Time: %lus", static_cast<unsigned long>((remaining_ms + 999U) / 1000U));
        display_.drawStr(0, 24, line);
        display_.drawStr(0, 38, contact ? "PPG: contact" : "PPG: no contact");
        display_.drawStr(0, 52, lead_off ? "ECG: leads off" : "ECG: connected");
    } else if (state == MeasurementState::Result) {
        metricText(line, sizeof(line), "HR", result.feature_packet.metrics.ecg_heart_rate_bpm);
        display_.drawStr(0, 21, line);
        metricText(line, sizeof(line), "Pulse", result.feature_packet.metrics.ppg_pulse_rate_bpm);
        display_.drawStr(0, 31, line);
        if (result.feature_packet.metrics.spo2.status == ValueStatus::Valid) {
            std::snprintf(line, sizeof(line), "SpO2 est %.1f%%", result.feature_packet.metrics.spo2.percent);
        } else {
            std::snprintf(line, sizeof(line), "SpO2 N/A");
        }
        display_.drawStr(0, 41, line);
        const char* stress = "Stress: N/A";
        if (result.stress.status == InferenceStatus::Valid)
            stress = result.stress.label == BranchLabel::Stress ? "Stress: HIGH" : "Stress: BASELINE";
        display_.drawStr(0, 51, stress);
        const char* decision = "NOT READY";
        if (result.remeasure) {
            decision = "REMEASURE";
        } else if (result.normal) {
            decision = "NORMAL";
        } else if (result.rhythm.label == BranchLabel::Af) {
            decision = "AF SCREEN ALERT";
        } else if (result.rhythm.label == BranchLabel::NonAf) {
            decision = "NON-AF SCREEN";
        }
        display_.drawStr(0, 62, decision);
    } else if (state == MeasurementState::ContactWait) {
        display_.drawStr(0, 26, ppg_available ? "Finger on MAX30102" : "MAX30102 missing");
        display_.drawStr(0, 42, lead_off ? "Attach ECG leads" : "ECG ready");
        display_.drawStr(0, 58, "BTN2: cancel");
    } else if (state == MeasurementState::Idle) {
        display_.drawStr(0, 28, "BTN1: start");
        display_.drawStr(0, 46, "PPG + ECG monitor");
    } else if (state == MeasurementState::Error) {
        display_.drawStr(0, 28, "Check wiring/power");
        display_.drawStr(0, 46, "BTN1: retry");
    }
    display_.sendBuffer();
#else
    (void)state;
    (void)result;
    (void)remaining_ms;
    (void)ppg_available;
    (void)contact;
    (void)lead_off;
#endif
}

}
