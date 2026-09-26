#pragma once

#include <cstdint>

namespace ppgfw {

enum class MeasurementState : uint8_t {
    Boot,
    SelfTest,
    Idle,
    ContactWait,
    Warmup,
    Measuring,
    QualityEvaluation,
    Result,
    Error
};

class MeasurementStateMachine {
public:
    void begin(uint64_t now_us);
    void acquisitionFailed(uint64_t now_us) { transition(MeasurementState::Error, now_us); }
    void selfTestComplete(bool success, uint64_t now_us);
    void update(uint64_t now_us, bool start_pressed, bool cancel_pressed, bool contact,
                bool lead_off, bool window_drained = true);
    void qualityEvaluated(uint64_t now_us);
    uint64_t measurementStartUs() const { return measurement_start_us_; }
    uint64_t measurementEndUs() const;
    MeasurementState state() const;
    uint32_t enteredAtMs() const;
    uint32_t remainingMs(uint64_t now_us) const;

private:
    void transition(MeasurementState next, uint64_t now_us);

    uint64_t measurement_start_us_{};
    MeasurementState state_{MeasurementState::Boot};
    uint64_t entered_at_us_{};
    uint64_t stable_contact_at_us_{};
    uint64_t contact_deadline_us_{};
};

}

