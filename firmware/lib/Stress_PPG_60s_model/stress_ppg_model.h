#pragma once
// Stress PPG model, 60 s: numerical inference ONLY from 14 ordered features.
// WESAD wrist BVP 64 Hz preprocessing is NOT performed in this header.
#include <cmath>
#include <cstddef>

namespace stress_ppg {
constexpr std::size_t kFeatureCount = 14;
constexpr double kThreshold = 0.5;
constexpr const char* kFeatureNames[kFeatureCount] = {
    "mean_hr_bpm", "std_hr_bpm", "min_hr_bpm", "max_hr_bpm",
    "mean_pp_ms", "median_pp_ms", "sdnn_ms", "rmssd_ms",
    "sdsd_ms", "pnn20_pct", "pnn50_pct", "cvnn",
    "beat_count", "valid_rr_ratio"};
constexpr double kMean[kFeatureCount] = {
        85.538969910079089, 10.997888455927679, 66.474298900491462, 114.35752474847651,
        746.97050149977133, 748.17904135338347, 92.36210793737979, 117.53399640576185,
        118.52623101143305, 73.380925324090086, 50.063103533049222, 0.12356184777088985,
        82.46616541353383, 0.80189941709243495};
constexpr double kScale[kFeatureCount] = {
        19.087725965650101, 3.491376103661072, 13.966148689547619, 24.579288875101707,
        157.15559967511797, 163.431607913954, 29.282728066812862, 44.065299313794974,
        44.564485929959609, 15.105510240532888, 19.525445682053491, 0.030155255006032295,
        13.975030833586953, 0.10192352573253306};
constexpr double kCoef[kFeatureCount] = {
        0.36297289985538578, 0.39709028015492559, 0.35281669728168014, 0.28595193477667602,
        -0.48537853666044223, -0.48517694938852102, -0.10419009428583353, -0.14129194337224071,
        -0.14137417274623246, 0.59190795371934102, 0.52856733009121804, 0.33562291693264412,
        0.35143020389649315, -0.10324555108196304};
constexpr double kIntercept = 0.15660050028627973;

struct Result {
    bool valid;
    double stress_probability;
    bool stress;
};

// Only call after the signal and PP-interval QC of the 60 s pipeline passed.
// Input units are identical to kFeatureNames and README.md.
inline Result infer(const double* features, std::size_t count) {
    if (features == nullptr || count != kFeatureCount) return {false, 0.0, false};
    double z = kIntercept;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(features[i]) || !std::isfinite(kScale[i]) || kScale[i] <= 0)
            return {false, 0.0, false};
        z += ((features[i] - kMean[i]) / kScale[i]) * kCoef[i];
    }
    if (!std::isfinite(z)) return {false, 0.0, false};
    const double probability = z >= 0 ? 1.0 / (1.0 + std::exp(-z))
                                      : std::exp(z) / (1.0 + std::exp(z));
    return {true, probability, probability >= kThreshold};
}
} // namespace stress_ppg
