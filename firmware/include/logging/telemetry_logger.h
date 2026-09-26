#pragma once

#include "types/result_types.h"
#include "types/runtime_types.h"

namespace ppgfw {

class TelemetryLogger {
public:
    void logDiagnostics(const IntegrityDiagnostics& ppg, const IntegrityDiagnostics& ecg);
    void logResult(const ResultSnapshot& result);
};

}

