#include <cstdio>
#include <cstdlib>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static float s_features[9];

static int get_data(size_t offset, size_t length, float* out) {
    for (size_t i = 0; i < length; i++) {
        out[i] = s_features[offset + i];
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 10) {
        printf("Usage: ecg_cpp_checker f1 .. f9\n");
        return 1;
    }
    for (int i = 0; i < 9; i++) {
        s_features[i] = (float)atof(argv[i+1]);
    }
    signal_t signal;
    signal.total_length = 9;
    signal.get_data = &get_data;

    ei_impulse_result_t result = {};
    EI_IMPULSE_ERROR err = process_impulse(&ei_default_impulse, &signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        printf("ERROR %d\n", err);
        return 2;
    }
    float p_af = result.classification[0].value;
    float p_nonaf = result.classification[1].value;
    const char* label = (p_af > p_nonaf) ? "AF" : "non-AF";
    printf("prob_af=%.6f prob_nonaf=%.6f pred=%s\n", p_af, p_nonaf, label);
    return 0;
}
