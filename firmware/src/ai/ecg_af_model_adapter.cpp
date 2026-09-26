#include "ai/ecg_af_model_adapter.h"

#include <cmath>

#if !defined(PPGFW_NATIVE_TEST)
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include <cstring>
#endif

namespace ppgfw {

#if !defined(PPGFW_NATIVE_TEST)
namespace {

static_assert(EI_CLASSIFIER_NN_INPUT_FRAME_SIZE == config::model::kEcgAfFeatureCount);
static_assert(EI_CLASSIFIER_LABEL_COUNT == 2);

std::array<float, config::model::kEcgAfFeatureCount> input_features{};
bool input_valid{};

int getData(size_t offset, size_t length, float* output) {
    if (!input_valid || output == nullptr ||
        offset + length > input_features.size()) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        output[index] = input_features[offset + index];
    }
    return 0;
}

}
#endif

EcgAfModelResult EcgAfModelAdapter::infer(
    const std::array<float, config::model::kEcgAfFeatureCount>& features) {
    EcgAfModelResult output{};
    for (float value : features) {
        if (!std::isfinite(value)) {
            return output;
        }
    }
#if defined(PPGFW_NATIVE_TEST)
    return output;
#else
    static constexpr const char* kExpectedAxes =
        "mean_rr + median_rr + sdnn + rmssd + pnn50 + cv_rr + iqr_rr + min_rr + max_rr";
    if (std::strcmp(EI_CLASSIFIER_FUSION_AXES_STRING, kExpectedAxes) != 0 ||
        std::strcmp(ei_classifier_inferencing_categories[0], "AF") != 0 ||
        std::strcmp(ei_classifier_inferencing_categories[1], "non-AF") != 0) {
        output.error = true;
        return output;
    }

    input_features = features;
    input_valid = true;
    signal_t signal{};
    signal.total_length = input_features.size();
    signal.get_data = &getData;
    ei_impulse_result_t result{};
    output.model_available = true;
    const EI_IMPULSE_ERROR error =
        process_impulse(&ei_default_impulse, &signal, &result, false);
    input_valid = false;
    if (error != EI_IMPULSE_OK) {
        output.error = true;
        return output;
    }
    output.probability_af = result.classification[0].value;
    output.probability_non_af = result.classification[1].value;
    output.valid = std::isfinite(output.probability_af) &&
                   std::isfinite(output.probability_non_af);
    return output;
#endif
}

}
