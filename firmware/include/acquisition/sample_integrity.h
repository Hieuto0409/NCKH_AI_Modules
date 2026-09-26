#pragma once

#include "types/raw_samples.h"
#include "types/runtime_types.h"

namespace ppgfw {

class SampleIntegrity {
public:
    static uint16_t ppgFlags(const PpgFifoSample& sample, bool overflow_context);
    static uint16_t ecgFlags(int16_t raw, bool lead_off, bool dropout_context);
    static float completeness(const IntegrityDiagnostics& diagnostics);
};

}

