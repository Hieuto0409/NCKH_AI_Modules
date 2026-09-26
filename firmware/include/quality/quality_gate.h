#pragma once

#include "types/quality_types.h"

namespace ppgfw {

class QualityGate {
public:
    static bool permitsPpgMetrics(const QualitySummary& quality);
    static bool permitsEcgMetrics(const QualitySummary& quality);
    static bool permitsFusion(const QualitySummary& ppg, const QualitySummary& ecg);
};

}

