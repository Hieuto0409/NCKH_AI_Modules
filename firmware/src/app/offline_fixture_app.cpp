#include "app/offline_fixture_app.h"

#if defined(PPGFW_OFFLINE_FIXTURE)

#include "ai/rhythm_engine.h"
#include "ai/stress_engine.h"
#include "config/board_config.h"
#include "config/sampling.h"
#include "config/versions.h"
#include "metrics/spo2_rate_adapter.h"

#include <Arduino.h>
#include <cmath>

namespace ppgfw {

void OfflineFixtureApp::begin() {
    Serial.begin(config::kSerialBaud);
    delay(250);
    Serial.println("*** OFFLINE TEST \xE2\x80\x94 NOT A SENSOR MEASUREMENT ***");
    Serial.printf("AI commit: %s\n", config::kAiRepositoryCommit);

    FeaturePacket stress_packet{};
    stress_packet.ppg_quality.status = QualityStatus::Good;
    stress_packet.stress_ppg.status = FeatureVectorStatus::Ready;
    stress_packet.stress_ppg.values = {
        90.09228867656934, 9.035973070988689, 72.45283018867924,
        123.87096774193549, 671.875, 671.875, 60.48228723909283,
        87.13422523306909, 87.85392657484061, 68.85245901639344,
        40.98360655737705, 0.09002014844888234, 92.0,
        0.6813186813186813};
    const BranchResult stress = StressEngine{}.infer(stress_packet);
    Serial.printf("Stress fixture: status=%u p=%.9f label=%u latency_us=%lu\n",
                  static_cast<unsigned>(stress.status), stress.score,
                  static_cast<unsigned>(stress.label),
                  static_cast<unsigned long>(stress.inference_latency_us));

    FeaturePacket ecg_packet{};
    ecg_packet.ecg_quality.status = QualityStatus::Good;
    ecg_packet.ecg_af.status = FeatureVectorStatus::Ready;
    ecg_packet.ecg_af.values = {0.82F, 0.81F, 0.04F, 0.03F, 20.0F,
                                0.05F, 0.05F, 0.72F, 0.95F};
    const BranchResult rhythm = RhythmEngine{}.infer(ecg_packet);
    Serial.printf("ECG AF fixture: status=%u label=%u p_af=%.6f latency_us=%lu\n",
                  static_cast<unsigned>(rhythm.status),
                  static_cast<unsigned>(rhythm.label), rhythm.score,
                  static_cast<unsigned long>(rhythm.inference_latency_us));

    Spo2RateAdapter200To100 spo2{};
    spo2.reset({config::kPpgSampleRateHz, config::kMax30102RedLedCurrent,
                config::kMax30102IrLedCurrent, config::kMax30102AdcRangeNa});
    for (uint32_t index = 0; index < 800; ++index) {
        const double phase = static_cast<double>(index) * 2.0 * 3.141592653589793 / 40.0;
        PpgSample sample{};
        sample.timestamp_us = static_cast<uint64_t>(index) * config::kPpgSamplePeriodUs;
        sample.seq = index;
        sample.ir = static_cast<uint32_t>(60000.0 + 1200.0 * std::sin(phase));
        sample.red = static_cast<uint32_t>(40000.0 + 500.0 * std::sin(phase));
        sample.flags = SampleValid | SampleContact;
        spo2.push(sample);
    }
    const Spo2Result oxygen = spo2.evaluate(true);
    Serial.printf("SpO2 fixture: status=%u estimator=%u percent=%.1f pairs=%lu resets=%lu\n",
                  static_cast<unsigned>(oxygen.status),
                  static_cast<unsigned>(oxygen.estimator_status), oxygen.percent,
                  static_cast<unsigned long>(oxygen.module_pair_count),
                  static_cast<unsigned long>(oxygen.adapter_reset_count));
    Serial.println("*** OFFLINE TEST COMPLETE ***");
}

void OfflineFixtureApp::loop() {
    delay(10000);
}

}

#endif
