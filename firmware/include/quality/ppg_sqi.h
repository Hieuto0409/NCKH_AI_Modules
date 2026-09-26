#pragma once

#include "dsp/ac_dc_estimator.h"
#include "types/quality_types.h"
#include "types/runtime_types.h"

#include <cstdint>

namespace ppgfw {

struct PpgQualityInput {
    IntegrityDiagnostics integrity{};
    AcDcSnapshot signal{};
    uint16_t peak_count{};
    bool contact_seen{};
};

class PpgSqi {
public:
    static QualitySummary evaluate(const PpgQualityInput& input);
};

}

