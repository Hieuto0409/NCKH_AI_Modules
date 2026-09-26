#include "logging/binary_logger.h"
#include "network/telemetry_publisher.h"
#include "config/versions.h"
#include <Arduino.h>
#include <fstream>
#include <iostream>
FakeSerial Serial;
namespace ppgfw {
MqttClient::MqttClient() {}
bool MqttClient::connected() {return true;}
bool MqttClient::publish(const char* data) {std::cout<<data<<'\n';return true;}
}
int main() {
    using namespace ppgfw;
    ResultSnapshot r; r.timestamp_us=64000000;
    auto& p=r.feature_packet;
    p.ppg_window.start_timestamp_us=4000000;p.ppg_window.end_timestamp_us=64000000;
    p.ecg_window.start_timestamp_us=9000000;p.ecg_window.end_timestamp_us=39000000;
    p.spo2_window.start_timestamp_us=60000000;p.spo2_window.end_timestamp_us=64000000;
    p.ppg_window.quality.status=p.ecg_window.quality.status=QualityStatus::Good;
    p.ppg_quality.status=p.ecg_quality.status=QualityStatus::Good;
    p.ppg_quality.score=.9F;p.ecg_quality.score=.8F;
    p.ppg_window.integrity.received_samples=12000;p.ecg_window.integrity.received_samples=15000;
    p.stress_ppg.status=p.ecg_af.status=FeatureVectorStatus::Ready;
    p.stress_ppg.beat_count=61;p.stress_ppg.raw_interval_count=60;p.stress_ppg.clean_interval_count=58;p.ecg_af.interval_count=29;
    r.stress.status=r.rhythm.status=InferenceStatus::Valid;
    r.stress.label=BranchLabel::Stress;r.stress.score=.72F;r.stress.confidence=.72F;
    r.rhythm.label=BranchLabel::NonAf;r.rhythm.score=.13F;r.rhythm.confidence=.87F;
    MqttClient client;TelemetryPublisher(client).publish(r);
    BinaryLogger logger;logger.begin(1000000,123);logger.enqueue(r);logger.flush(100);
    std::ofstream f("capture.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(Serial.bytes.data()),Serial.bytes.size());
}
