#pragma once

#include "ai/inference_engine.h"

namespace ppgfw {

class RhythmEngine final : public IInferenceEngine {
public:
    BranchResult infer(const FeaturePacket& packet) const override;
};

}

