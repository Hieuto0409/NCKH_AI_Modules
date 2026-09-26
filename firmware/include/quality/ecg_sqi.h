#pragma once

#include "types/metric_types.h"
#include "types/quality_types.h"
#include "types/runtime_types.h"

#include <cstdint>

namespace ppgfw {

struct EcgQualityInput {
    IntegrityDiagnostics integrity{};
    SignalStatistics signal{};
    uint16_t peak_count{};
};

class EcgSqi {
public:
    static QualitySummary evaluate(const EcgQualityInput& input);
};

}

