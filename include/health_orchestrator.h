/**
 * @file health_orchestrator.h
 * @brief Coordination layer for three independent health-monitoring branches.
 *
 * Three branches are intentionally kept separate.
 * Each branch exposes its own ready/not-ready state.
 * No branch produces a result unless it has received the correct, validated inputs.
 *
 * Branch 1 – Stress PPG:
 *   Input : 14 HRV features (double[14]), in exact order defined by stress_ppg::kFeatureNames.
 *   Source: Signal-processing module (nhóm trưởng), 60-s PPG window → features.
 *   NOT_READY if features not yet provided.
 *
 * Branch 2 – ECG AF/non-AF (Edge Impulse):
 *   Input : 9 ECG-HRV features (float[9]), as EI_CLASSIFIER_FUSION_AXES_STRING:
 *           mean_rr, median_rr, sdnn, rmssd, pnn50, cv_rr, iqr_rr, min_rr, max_rr
 *   Source: Signal-processing module (nhóm trưởng), computed from R-peak intervals.
 *   NOT_READY if features not yet provided.
 *
 * Branch 3 – SpO₂:
 *   Input : Raw IR/RED ADC pairs at exactly 100 Hz (uint32_t each), plus an
 *           independent external quality flag (bool poorSignal from sensor).
 *   Source: MAX30102 FIFO at 100 Hz, sampleAverage=1.
 *   NOT_READY until 400 raw pairs (4 s) accumulated; QualityRejected if poor signal.
 *
 * IMPORTANT DISCLAIMERS (never remove):
 *   - Stress model: logistic regression trained on WESAD dataset (93.33% on test set).
 *     This is NOT clinical diagnosis of stress. NOT validated on MAX30102.
 *   - ECG: AF/non-AF classification only. NOT a general arrhythmia detector.
 *     NOT a medical diagnosis. NOT validated on this hardware.
 *   - SpO₂: Ratio-based estimation algorithm (Maxim/SparkFun). NOT clinically validated.
 *     Status::Ok means passed software checks only.
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <algorithm>

// Stress PPG — header-only logistic regression
#include "stress_ppg_model.h"

// SpO₂ — research estimation algorithm
#include "ResearchSpO2.h"

namespace orchestrator {

enum class ReadyState { NOT_READY, READY };

// ── Branch 1: Stress ─────────────────────────────────────────────────────────
struct StressResult {
    ReadyState       state          = ReadyState::NOT_READY;
    bool             valid          = false;   // model returned valid flag
    double           probability    = 0.0;     // [0,1]; only meaningful if valid
    bool             stress         = false;   // probability >= 0.5
    double           heart_rate_bpm = 0.0;     // PPG BPM (60000 / median_pp_ms); 0.0 if NOT_READY / invalid
    // Disclaimer: WESAD 93.33% test accuracy, not validated on wrist/MAX30102.
};

/// Feed exactly 14 features in the order defined by stress_ppg::kFeatureNames.
/// Caller must have completed 60-s PPG window and PP-interval QC first.
/// Pass nullptr or wrong count → returns NOT_READY (no crash).
StressResult runStress(const double* features14, std::size_t count);

// ── PPG Heart Rate ───────────────────────────────────────────────────────────
struct PpgHeartRateResult {
    ReadyState  state            = ReadyState::NOT_READY;
    bool        valid            = false;
    double      heart_rate_bpm   = 0.0; // BPM (60000.0 / median_ppi_ms); 0.0 if NOT_READY
    double      median_ppi_ms    = 0.0;
    std::size_t valid_beat_count = 0;
};

/// Calculate PPG Heart Rate from an array of valid peak-to-peak intervals (in ms).
/// Formula: 60000.0 / median(valid_ppi_ms).
/// Requires count >= min_intervals (default 4).
/// Returns NOT_READY (valid=false, bpm=0.0) if ppi_ms == nullptr or count < min_intervals.
inline PpgHeartRateResult computePpgHeartRate(const double* ppi_ms, std::size_t count, std::size_t min_intervals = 4) {
    PpgHeartRateResult out;
    if (ppi_ms == nullptr || count < min_intervals) {
        return out; // NOT_READY
    }
    std::vector<double> valid;
    valid.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (std::isfinite(ppi_ms[i]) && ppi_ms[i] >= 333.33 && ppi_ms[i] <= 1500.0) {
            valid.push_back(ppi_ms[i]);
        }
    }
    if (valid.size() < min_intervals) {
        return out; // NOT_READY
    }
    std::sort(valid.begin(), valid.end());
    double median_ppi = 0.0;
    std::size_t n = valid.size();
    if (n % 2 == 1) {
        median_ppi = valid[n / 2];
    } else {
        median_ppi = 0.5 * (valid[n / 2 - 1] + valid[n / 2]);
    }
    if (median_ppi <= 0.0) {
        return out;
    }
    out.state            = ReadyState::READY;
    out.valid            = true;
    out.median_ppi_ms    = median_ppi;
    out.heart_rate_bpm   = 60000.0 / median_ppi;
    out.valid_beat_count = n;
    return out;
}


// ── Branch 2: ECG AF/non-AF ──────────────────────────────────────────────────
struct EcgResult {
    ReadyState  state      = ReadyState::NOT_READY;
    bool        valid      = false;
    float       prob_af    = 0.0f;   // P(AF);   only meaningful if valid
    float       prob_nonaf = 0.0f;   // P(non-AF)
    bool        is_af      = false;  // prob_af > prob_nonaf
    // Disclaimer: AF/non-AF classification only. NOT a medical diagnosis.
};

/// Feed exactly 9 features in EI_CLASSIFIER_FUSION_AXES_STRING order:
///   mean_rr, median_rr, sdnn, rmssd, pnn50, cv_rr, iqr_rr, min_rr, max_rr
/// Units: NOT assumed here — caller must match training data units exactly.
/// Pass nullptr or wrong count → returns NOT_READY.
EcgResult runEcg(const float* features9, std::size_t count);


// ── Branch 3: SpO₂ ───────────────────────────────────────────────────────────
struct Spo2Result {
    ReadyState              state    = ReadyState::NOT_READY;
    research_spo2::Result   detail;  // contains status, percent, heartRate
    // detail.valid() == true only when state == READY && algorithm succeeded.
    // Disclaimer: Ratio-based estimation, not clinically validated.
};

/// Call at session start, FIFO overflow, gap, or rate/LED changes.
void        resetSpo2();
/// Push exactly one raw IR/RED pair from sensor FIFO at 100 Hz.
void        pushSpo2(uint32_t ir, uint32_t red);
/// Evaluate with current external quality flag (!poorSignal from sensor).
/// Returns NOT_READY until 400 raw pairs (4 s) have been pushed since last reset.
Spo2Result  evaluateSpo2(bool externalQualityOk);

} // namespace orchestrator
