#pragma once
#include "features/ecg_af_feature_adapter.h"
#include "types/runtime_types.h"
#include "types/raw_samples.h"
#include "utils/running_statistics.h"
#include "config/sampling.h"
#include <array>
namespace ppgfw {
struct EcgWindowResult {
    WindowMetadata metadata{};
    EcgAfFeatureVector features{};
    SignalStatistics statistics{};
    float mean_peak_amplitude{};
    uint16_t peak_count{};
};
class EcgWindowBuilder {
public:
    static constexpr uint64_t kDurationUs = 30000000;
    static constexpr uint64_t kStepUs = 5000000;
    void reset(uint64_t start_us);
    void addSample(const EcgSample& sample, float processed);
    void addPeak(uint64_t timestamp_us, float amplitude);
    void breakContinuity(uint64_t timestamp_us);
    EcgWindowResult build(uint64_t end_us) const;
private:
    struct Candidate {
        uint64_t start{}, last_peak{};
        bool have_peak{}, have_sequence{};
        uint32_t last_sequence{};
        IntegrityDiagnostics integrity{};
        RunningStatistics signal{}, amplitudes{};
        std::array<float, config::kIntervalCapacity> rr{};
        size_t count{};
        uint16_t peaks{};
    };
    std::array<Candidate, 7> candidates_{};
};
}
