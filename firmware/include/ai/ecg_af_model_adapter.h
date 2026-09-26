#pragma once

#include "config/model_contracts.h"

#include <array>

namespace ppgfw {

struct EcgAfModelResult {
    bool valid{};
    bool model_available{};
    bool error{};
    float probability_af{};
    float probability_non_af{};
};

class EcgAfModelAdapter {
public:
    static EcgAfModelResult infer(
        const std::array<float, config::model::kEcgAfFeatureCount>& features);
};

}
