#pragma once

#include "types/result_types.h"

namespace ppgfw {

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    virtual BranchResult infer(const FeaturePacket& packet) const = 0;
};

}

