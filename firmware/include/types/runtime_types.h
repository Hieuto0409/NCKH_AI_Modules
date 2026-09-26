#pragma once

#include "types/quality_types.h"

#include <cstdint>

namespace ppgfw {

struct IntegrityDiagnostics {
    uint32_t expected_samples{};
    uint32_t received_samples{};
    uint32_t dropped_samples{};
    uint32_t overflow_count{};
    uint32_t clipping_count{};
    uint32_t lead_off_count{};
    uint32_t queue_drop_count{};
    uint32_t queue_high_water_mark{};
    uint32_t worst_poll_latency_us{};
};

struct WindowMetadata {
    uint64_t start_timestamp_us{};
    uint64_t end_timestamp_us{};
    IntegrityDiagnostics integrity{};
    QualitySummary quality{};
    uint16_t config_version{};
};

}

