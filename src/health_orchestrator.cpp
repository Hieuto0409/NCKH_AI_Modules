/**
 * @file health_orchestrator.cpp
 * @brief Implementation of the three-branch health-monitoring orchestration layer.
 *
 * Branch isolation rules:
 *   - Stress PPG: calls stress_ppg::infer() with exactly 14 double features.
 *   - ECG AF/non-AF: calls Edge Impulse process_impulse() with 9 float features via
 *     a get_data callback. No raw signal processing is done here.
 *   - SpO2: delegates to a static research_spo2::Stream100 object. Caller controls
 *     100 Hz push rate and provides the independent quality flag.
 *
 * DISCLAIMERS (must not be removed):
 *   - Stress: WESAD 93.33% test-set, not validated on MAX30102.
 *   - ECG: AF/non-AF only, not a medical arrhythmia diagnosis tool.
 *   - SpO2: ratio-based estimation, not clinically validated.
 */

#include "health_orchestrator.h"

// ─── Branch 2: Edge Impulse ECG ───────────────────────────────────────────────
// The Edge Impulse SDK uses a signal_t + get_data callback pattern.
// We include the run-classifier header which pulls model_variables.h transitively.
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include <cmath>     // std::isfinite — for ECG and PPI validation
#include <vector>    // for computePpgHeartRate
#include <algorithm> // std::sort

namespace orchestrator {

// ─── Branch 1: Stress ─────────────────────────────────────────────────────────
StressResult runStress(const double* features14, std::size_t count) {
    StressResult out;
    if (features14 == nullptr || count != stress_ppg::kFeatureCount) {
        // Caller hasn't provided 14 valid features yet — stay NOT_READY.
        return out;
    }
    auto r = stress_ppg::infer(features14, count);
    out.state          = ReadyState::READY;
    out.valid          = r.valid;
    out.probability    = r.stress_probability;
    out.stress         = r.stress;
    // Feature 5 is median_pp_ms in stress_ppg::kFeatureNames
    // Robust session BPM = 60000.0 / median_pp_ms; fallback to feature 0 (mean_hr_bpm)
    if (features14[5] > 0.0) {
        out.heart_rate_bpm = 60000.0 / features14[5];
    } else if (features14[0] > 0.0) {
        out.heart_rate_bpm = features14[0];
    }
    return out;
}

// ─── Branch 2: ECG AF/non-AF ──────────────────────────────────────────────────
// Edge Impulse signal_t requires a get_data function pointer.
// We store the caller's 9 features in a static buffer for the duration of one call.
static float  s_ecg_features[EI_CLASSIFIER_NN_INPUT_FRAME_SIZE]; // = 9
static bool   s_ecg_valid = false; // set to false on any validation error

static int ecg_get_data(size_t offset, size_t length, float* out) {
    // Guard: offset+length must not exceed the feature buffer.
    // EI SDK should never call this with out-of-bounds, but check defensively.
    if (!s_ecg_valid || out == nullptr ||
        offset + length > (size_t)EI_CLASSIFIER_NN_INPUT_FRAME_SIZE) {
        return -1; // signal error to EI runtime
    }
    for (size_t i = 0; i < length; i++) {
        if (!std::isfinite(s_ecg_features[offset + i])) return -1;
        out[i] = s_ecg_features[offset + i];
    }
    return 0;
}

EcgResult runEcg(const float* features9, std::size_t count) {
    EcgResult out; // default: NOT_READY, valid=false
    s_ecg_valid = false;

    if (features9 == nullptr || count != EI_CLASSIFIER_NN_INPUT_FRAME_SIZE) {
        return out; // NOT_READY — caller has not provided 9 features yet
    }
    // Validate all 9 features are finite before passing to EI runtime.
    for (size_t i = 0; i < EI_CLASSIFIER_NN_INPUT_FRAME_SIZE; i++) {
        if (!std::isfinite(features9[i])) {
            return out; // NOT_READY — non-finite input; do not call EI
        }
    }
    // Copy into static buffer (not re-entrant; single-core MCU use only).
    for (size_t i = 0; i < EI_CLASSIFIER_NN_INPUT_FRAME_SIZE; i++) {
        s_ecg_features[i] = features9[i];
    }
    s_ecg_valid = true;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_NN_INPUT_FRAME_SIZE;
    signal.get_data     = &ecg_get_data;

    ei_impulse_result_t result = {};
    // EI_IMPULSE_RESULT_CLASSIFICATION_IS_STATICALLY_ALLOCATED == 1 for this model,
    // so classification[] is embedded in the struct. No dynamic allocation needed.
    EI_IMPULSE_ERROR err = process_impulse(&ei_default_impulse, &signal, &result, false);

    out.state = ReadyState::READY; // algorithm ran (result may or may not be valid)
    if (err != EI_IMPULSE_OK) {
        // EI internal error: leave valid=false, do not expose stale result.
        out.valid = false;
        return out;
    }
    // Categories: index 0 = "AF", index 1 = "non-AF" (from model_variables.h line 50).
    out.valid      = true;
    out.prob_af    = result.classification[0].value;   // "AF"
    out.prob_nonaf = result.classification[1].value;   // "non-AF"
    out.is_af      = (out.prob_af > out.prob_nonaf);
    return out;
}


// ─── Branch 3: SpO₂ ───────────────────────────────────────────────────────────
static research_spo2::Stream100 s_spo2_stream;

void resetSpo2() {
    s_spo2_stream.reset();
}

void pushSpo2(uint32_t ir, uint32_t red) {
    s_spo2_stream.push(ir, red);
}

Spo2Result evaluateSpo2(bool externalQualityOk) {
    Spo2Result out;
    auto r = s_spo2_stream.evaluate(externalQualityOk);
    if (r.status == research_spo2::Status::NeedData) {
        out.state  = ReadyState::NOT_READY;
        out.detail = r;
        return out;
    }
    out.state  = ReadyState::READY; // READY means algorithm ran (not necessarily valid)
    out.detail = r;
    return out;
}

} // namespace orchestrator
