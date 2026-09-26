#include "metrics/spo2.h"

#include "config/thresholds.h"
#include "config/versions.h"
#include "metrics/spo2_calibration.h"
#include "quality/quality_gate.h"

#include <algorithm>
#include <cmath>

namespace ppgfw {

Spo2Result RatioOfRatiosSpo2::compute(const AcDcSnapshot& signal,
                                      const QualitySummary& quality) const {
    Spo2Result output{};
    output.algorithm_version = config::kSpo2AlgorithmVersion;
    output.calibration_version = config::kSpo2CalibrationVersion;
    output.red_ac = signal.red_ac;
    output.red_dc = signal.red_dc;
    output.ir_ac = signal.ir_ac;
    output.ir_dc = signal.ir_dc;
    if (!QualityGate::permitsPpgMetrics(quality)) {
        output.status = ValueStatus::InvalidSignal;
        return output;
    }
    if (signal.red_dc <= 0.0F || signal.ir_dc <= 0.0F || signal.ir_ac <= 0.0F) {
        output.status = ValueStatus::InvalidSignal;
        return output;
    }
    output.ratio = (signal.red_ac / signal.red_dc) / (signal.ir_ac / signal.ir_dc);
    if (!std::isfinite(output.ratio) || output.ratio < config::candidate::kSpo2RatioMinimum ||
        output.ratio > config::candidate::kSpo2RatioMaximum) {
        output.status = ValueStatus::InvalidSignal;
        return output;
    }
    const float candidate = config::candidate::kSpo2PolynomialA * output.ratio +
                            config::candidate::kSpo2PolynomialB;
    if (!Spo2Calibration::validated()) {
        output.status = ValueStatus::NotAvailable;
        output.percent = kUnavailableValue;
        return output;
    }
    output.percent = std::clamp(Spo2Calibration::apply(candidate), 0.0F, 100.0F);
    output.status = ValueStatus::Valid;
    return output;
}

}

