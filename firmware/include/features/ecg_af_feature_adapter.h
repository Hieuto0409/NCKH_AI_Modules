#pragma once

#include "types/model_feature_types.h"

#include <cstddef>

namespace ppgfw {

class EcgAfFeatureAdapter {
public:
    static EcgAfFeatureVector build(const float* intervals_ms, size_t count);
};

}
