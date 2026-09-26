#include "ai/noop_engine.h"

#include "config/versions.h"

namespace ppgfw {

BranchResult NoopInferenceEngine::infer(const FeaturePacket&) const {
    BranchResult result{};
    result.status = InferenceStatus::NotReady;
    result.model_version = config::kModelVersionUnavailable;
    result.scaler_version = config::kScalerVersionUnavailable;
    return result;
}

}
