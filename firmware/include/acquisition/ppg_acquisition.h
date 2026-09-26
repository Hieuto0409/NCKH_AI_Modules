#pragma once

#include "acquisition/sample_queues.h"
#include "drivers/max30102_driver.h"
#include "types/runtime_types.h"

#include <cstdint>

namespace ppgfw {

class PpgAcquisition {
public:
    PpgAcquisition(Max30102Driver& driver, PpgSampleQueue& queue);
    bool begin();
    void poll();
    bool setActive(bool active);
    void resetWindowDiagnostics();
    IntegrityDiagnostics diagnostics() const;
    bool sensorAvailable() const;
    bool contactDetected() const;
    uint32_t latestIr() const;

private:
    Max30102Driver& driver_;
    PpgSampleQueue& queue_;
    IntegrityDiagnostics diagnostics_{};
    uint32_t sequence_{};
    uint64_t last_timestamp_us_{};
    uint64_t last_poll_us_{};
    bool pending_discontinuity_{};
    uint32_t latest_ir_{};
    bool available_{};
    bool active_{};
};

}

