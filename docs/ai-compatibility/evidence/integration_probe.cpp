#include "acquisition/timestamp_service.h"
#include "metrics/spo2_rate_adapter.h"
#include "features/stress_ppg_60s_adapter.h"
#include "features/ecg_af_feature_adapter.h"
#include "app/measurement_state_machine.h"
#include "config/sampling.h"
#include <stress_ppg_model.h>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <string>

void check_stream(uint64_t poll_drift_us) {
    using namespace ppgfw;
    Spo2RateAdapter200To100 adapter;
    adapter.reset({200, 0x3f, 0x3f, 8192});
    uint64_t last = 0;
    unsigned gaps = 0;
    for (unsigned batch = 0; batch < 200; ++batch) {
        auto plan = PpgTimestampReconstructor::plan(
            1000000ULL + batch * (20000ULL + poll_drift_us), 4, last, 5000);
        gaps += plan.inferred_gap_samples;
        for (unsigned j = 0; j < 4; ++j) {
            unsigned index = batch * 4 + j;
            double phase = index * 2.0 * 3.141592653589793 / 40.0;
            last = plan.first_timestamp_us + j * 5000;
            adapter.push({last, index,
                static_cast<uint32_t>(40000 + 500 * std::sin(phase)),
                static_cast<uint32_t>(60000 + 1200 * std::sin(phase)),
                SampleValid | SampleContact});
        }
    }
    auto result = adapter.evaluate(true);
    std::printf("poll_drift_us=%llu samples=800 inferred_gaps=%u resets=%u retained_pairs=%u need_data=%d\n",
        static_cast<unsigned long long>(poll_drift_us), gaps, adapter.resetCount(),
        adapter.modulePairCount(), result.estimator_status == Spo2EstimatorStatus::NeedData);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "stress") {
        double f[14];
        std::cout << std::setprecision(17);
        while (std::cin >> f[0]) {
            for (int i = 1; i < 14; ++i) if (!(std::cin >> f[i])) return 2;
            auto result = stress_ppg::infer(f, 14);
            std::cout << result.stress_probability << ' ' << result.stress << '\n';
        }
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "ecg_features") {
        size_t n; std::cout << std::setprecision(17);
        while (std::cin >> n) {
            float rr[256]; if(n>256) return 2;
            for(size_t i=0;i<n;++i) if(!(std::cin>>rr[i])) return 2;
            auto f=ppgfw::EcgAfFeatureAdapter::build(rr,n);
            std::cout<<unsigned(f.status); for(auto v:f.values) std::cout<<' '<<v; std::cout<<'\n';
        } return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "stress_peaks") {
        size_t n; std::cout << std::setprecision(17);
        while (std::cin >> n) {
            ppgfw::StressPpg60sAdapter a; a.reset(0);
            for(size_t i=0;i<n;++i) { uint64_t t; if(!(std::cin>>t)) return 2; a.addPeak(t); }
            auto f=a.build(60000000);
            std::cout<<unsigned(f.status); for(auto v:f.values) std::cout<<' '<<v; std::cout<<'\n';
        } return 0;
    }
    check_stream(0);
    check_stream(1);
    ppgfw::StressPpg60sAdapter stress;
    ppgfw::MeasurementStateMachine machine;
    machine.begin(0);
    machine.selfTestComplete(true, 0);
    machine.update(1000, true, false, true, false);
    machine.update(2000, false, false, true, false);
    machine.update(502000, false, false, true, false);
    machine.update(3502900, false, false, true, false);
    stress.reset(3502900);
    for (unsigned i = 0; i < 60; ++i) stress.addPeak(4000000ULL + i * 1000000ULL);
    machine.update(63502400, false, false, true, false);
    auto short_window = stress.build(63502400);
    auto full_window = stress.build(63502900);
    std::printf("state_machine_finished=%d stress_at_elapsed_59999500us_status=%u stress_at_elapsed_60000000us_status=%u (0=NotReady,1=Ready)\n",
        machine.state() == ppgfw::MeasurementState::QualityEvaluation,
        static_cast<unsigned>(short_window.status), static_cast<unsigned>(full_window.status));
    float rr[75];
    for (unsigned i=0;i<75;++i) rr[i] = i < 30 ? 1000.0f : 666.666687f;
    auto all = ppgfw::EcgAfFeatureAdapter::build(rr,75);
    auto last30 = ppgfw::EcgAfFeatureAdapter::build(rr+30,45);
    std::printf("ecg_60s_mean_rr=%.6f ecg_last30s_mean_rr=%.6f ecg_60s_sdnn=%.6f ecg_last30s_sdnn=%.6f\n",
        all.values[0], last30.values[0], all.values[2], last30.values[2]);
}
