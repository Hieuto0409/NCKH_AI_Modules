#include "quality/quality_gate.h"

namespace ppgfw {

bool QualityGate::permitsPpgMetrics(const QualitySummary& quality) {
    return quality.status == QualityStatus::Good;
}

bool QualityGate::permitsEcgMetrics(const QualitySummary& quality) {
    return quality.status == QualityStatus::Good;
}

bool QualityGate::permitsFusion(const QualitySummary& ppg, const QualitySummary& ecg) {
    return permitsPpgMetrics(ppg) && permitsEcgMetrics(ecg);
}

}

