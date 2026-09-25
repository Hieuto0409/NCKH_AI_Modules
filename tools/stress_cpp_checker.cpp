/**
 * stress_cpp_checker.cpp
 * ----------------------
 * CLI wrapper that calls stress_ppg::infer() from stress_ppg_model.h.
 *
 * Usage:
 *   stress_cpp_checker.exe f0 f1 f2 ... f13
 *   (14 double feature values in kFeatureNames order)
 *
 * Output (one line to stdout):
 *   prob=0.19840123456789012 pred=baseline
 *
 * Exit codes:
 *   0  — inference completed successfully
 *   1  — wrong number of arguments or parse error
 *   2  — stress_ppg::infer() returned valid=false
 */
#include <cstdlib>
#include <cstdio>
#include "stress_ppg_model.h"

int main(int argc, char* argv[]) {
    if (argc != 1 + (int)stress_ppg::kFeatureCount) {
        fprintf(stderr,
                "Usage: stress_cpp_checker <f0> <f1> ... <f%zu>\n"
                "  (exactly %zu feature values in kFeatureNames order)\n",
                stress_ppg::kFeatureCount - 1,
                stress_ppg::kFeatureCount);
        return 1;
    }

    double features[stress_ppg::kFeatureCount];
    for (std::size_t i = 0; i < stress_ppg::kFeatureCount; ++i) {
        char* end = nullptr;
        features[i] = std::strtod(argv[1 + (int)i], &end);
        if (end == argv[1 + (int)i] || *end != '\0') {
            fprintf(stderr, "Parse error: argument %zu '%s' is not a valid double\n",
                    i + 1, argv[1 + (int)i]);
            return 1;
        }
    }

    stress_ppg::Result r = stress_ppg::infer(features, stress_ppg::kFeatureCount);
    if (!r.valid) {
        fprintf(stderr, "stress_ppg::infer() returned valid=false\n");
        return 2;
    }

    /* Print with 20 significant decimal places so Python can verify at 1e-9 */
    printf("prob=%.20f pred=%s\n",
           r.stress_probability,
           r.stress ? "stress" : "baseline");
    return 0;
}
