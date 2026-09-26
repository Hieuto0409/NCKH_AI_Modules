#pragma once

#include "ai/inference_engine.h"

namespace ppgfw {

class LowO2Engine final : public IInferenceEngine {
public:
    BranchResult infer(const FeaturePacket& packet) const override;
};

}

