#pragma once

#include "dsp/ac_dc_estimator.h"
#include "types/metric_types.h"
#include "types/quality_types.h"

namespace ppgfw {

class ISpo2Algorithm {
public:
    virtual ~ISpo2Algorithm() = default;
    virtual Spo2Result compute(const AcDcSnapshot& signal, const QualitySummary& quality) const = 0;
};

class RatioOfRatiosSpo2 final : public ISpo2Algorithm {
public:
    Spo2Result compute(const AcDcSnapshot& signal, const QualitySummary& quality) const override;
};

}

