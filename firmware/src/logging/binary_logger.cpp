#include "logging/binary_logger.h"

#include "config/board_config.h"
#include "config/versions.h"

#include <Arduino.h>
#include <cstddef>
#include <cstring>

namespace ppgfw {

namespace {

BinaryLogRecord makeRecord(BinaryRecordType type, uint64_t timestamp_us, uint32_t sequence,
                           int32_t value_a, int32_t value_b, uint16_t flags) {
    BinaryLogRecord record{};
    record.schema = static_cast<uint8_t>(config::kRawSchemaVersion);
    record.type = type;
    record.timestamp_us = timestamp_us;
    record.sequence = sequence;
    record.value_a = value_a;
    record.value_b = value_b;
    record.flags = flags;
    return record;
}

uint8_t hexNibble(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<uint8_t>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<uint8_t>(value - 'A' + 10);
    }
    return 0;
}

BinaryLogRecord makeAiProvenanceRecord() {
    uint8_t commit[20]{};
    for (size_t index = 0; index < sizeof(commit); ++index) {
        commit[index] = static_cast<uint8_t>(
            (hexNibble(config::kAiRepositoryCommit[index * 2U]) << 4U) |
            hexNibble(config::kAiRepositoryCommit[index * 2U + 1U]));
    }
    BinaryLogRecord record{};
    record.schema = static_cast<uint8_t>(config::kRawSchemaVersion);
    record.type = BinaryRecordType::AiProvenance;
    std::memcpy(&record.timestamp_us, commit, 8);
    std::memcpy(&record.sequence, commit + 8, 4);
    std::memcpy(&record.value_a, commit + 12, 4);
    std::memcpy(&record.value_b, commit + 16, 4);
    return record;
}

}

void BinaryLogger::begin(uint64_t session_id, uint64_t device_id) {
#if APP_ENABLE_BINARY_LOG
    BinaryLogRecord session = makeRecord(
        BinaryRecordType::Session, session_id, static_cast<uint32_t>(device_id),
        static_cast<int32_t>(device_id >> 32U),
        static_cast<int32_t>((config::kEcgSampleRateHz << 16U) | config::kPpgSampleRateHz),
        config::kFeatureSchemaVersion);
    enqueueRecord(session);
    BinaryLogRecord versions = makeRecord(
        BinaryRecordType::Config, session_id, config::kFirmwareVersionCode,
        static_cast<int32_t>((config::kPpgDspConfigVersion << 16U) |
                             config::kEcgDspConfigVersion),
        static_cast<int32_t>((config::kQualityConfigVersion << 16U) |
                             config::kSpo2AlgorithmVersion),
        config::kSpo2CalibrationVersion);
    enqueueRecord(versions);
    BinaryLogRecord hardware = makeRecord(
        BinaryRecordType::HardwareConfig, session_id,
        static_cast<uint32_t>(config::kMax30102RedLedCurrent) |
            (static_cast<uint32_t>(config::kMax30102IrLedCurrent) << 8U),
        config::kMax30102AdcRangeNa, static_cast<int32_t>(config::kMeasurementWindowMs),
        config::kMax30102PulseWidthUs);
    enqueueRecord(hardware);
    enqueueRecord(makeAiProvenanceRecord());
#else
    (void)session_id;
    (void)device_id;
#endif
}

bool BinaryLogger::enqueue(const PpgSample& sample) {
#if APP_ENABLE_BINARY_LOG
    BinaryLogRecord record = makeRecord(BinaryRecordType::Ppg, sample.timestamp_us, sample.seq,
                                        static_cast<int32_t>(sample.red),
                                        static_cast<int32_t>(sample.ir), sample.flags);
    return enqueueRecord(record);
#else
    (void)sample;
#endif
    return true;
}

bool BinaryLogger::enqueue(const EcgSample& sample) {
#if APP_ENABLE_BINARY_LOG
    BinaryLogRecord record = makeRecord(BinaryRecordType::Ecg, sample.timestamp_us, sample.seq,
                                        sample.raw, 0, sample.flags);
    return enqueueRecord(record);
#else
    (void)sample;
#endif
    return true;
}

bool BinaryLogger::enqueue(const ResultSnapshot& result) {
#if APP_ENABLE_BINARY_LOG
    auto bits = [](float value) {
        int32_t output{};
        std::memcpy(&output, &value, sizeof(output));
        return output;
    };
    const uint64_t timestamp = result.timestamp_us;
    bool success = true;
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummaryQuality, timestamp,
                                        result.feature_packet.ppg_quality.reasons |
                                            result.feature_packet.ecg_quality.reasons,
                                        bits(result.feature_packet.ppg_quality.score),
                                        bits(result.feature_packet.ecg_quality.score),
                                        static_cast<uint16_t>(result.feature_packet.ppg_quality.status) |
                                            (static_cast<uint16_t>(result.feature_packet.ecg_quality.status) << 8U)));
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummaryMetrics, timestamp, 0,
                                        bits(result.feature_packet.metrics.ecg_heart_rate_bpm.value),
                                        bits(result.feature_packet.metrics.ppg_pulse_rate_bpm.value),
                                        static_cast<uint16_t>(result.feature_packet.metrics.ecg_heart_rate_bpm.status) |
                                            (static_cast<uint16_t>(result.feature_packet.metrics.ppg_pulse_rate_bpm.status) << 8U)));
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummarySpo2, timestamp,
                                        (static_cast<uint32_t>(result.feature_packet.metrics.spo2.algorithm_version) << 16U) |
                                            result.feature_packet.metrics.spo2.calibration_version,
                                        bits(result.feature_packet.metrics.spo2.percent),
                                        bits(result.feature_packet.metrics.spo2.ratio),
                                        static_cast<uint16_t>(result.feature_packet.metrics.spo2.status)));
    const uint16_t decision_flags = static_cast<uint16_t>(result.stress.status) |
                                    (static_cast<uint16_t>(result.low_o2.status) << 3U) |
                                    (static_cast<uint16_t>(result.rhythm.status) << 6U) |
                                    (result.normal ? 1U << 9U : 0U) |
                                    (result.remeasure ? 1U << 10U : 0U);
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummaryDecision, timestamp,
                                        result.stress.reason_flags | result.low_o2.reason_flags |
                                            result.rhythm.reason_flags,
                                        bits(result.stress.score), bits(result.low_o2.score),
                                        decision_flags));
    const uint32_t stress_versions = static_cast<uint32_t>(result.stress.model_version) |
                                     (static_cast<uint32_t>(result.stress.scaler_version) << 16U);
    const uint32_t low_o2_versions = static_cast<uint32_t>(result.low_o2.model_version) |
                                     (static_cast<uint32_t>(result.low_o2.scaler_version) << 16U);
    const uint32_t rhythm_versions = static_cast<uint32_t>(result.rhythm.model_version) |
                                     (static_cast<uint32_t>(result.rhythm.scaler_version) << 16U);
    const uint16_t modality_flags = result.stress.required_modalities |
                                    (static_cast<uint16_t>(result.low_o2.required_modalities) << 4U) |
                                    (static_cast<uint16_t>(result.rhythm.required_modalities) << 8U);
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummaryModelVersions, timestamp,
                                        stress_versions, static_cast<int32_t>(low_o2_versions),
                                        static_cast<int32_t>(rhythm_versions), modality_flags));
    success &= enqueueRecord(makeRecord(
        BinaryRecordType::SummaryInferenceLatency, timestamp,
        result.stress.inference_latency_us,
        static_cast<int32_t>(result.low_o2.inference_latency_us),
        static_cast<int32_t>(result.rhythm.inference_latency_us), 0));
    auto window = [&](BinaryRecordType type, const WindowMetadata& w, uint8_t status) {
        success &= enqueueRecord(makeRecord(type, w.start_timestamp_us,
            static_cast<uint32_t>(w.end_timestamp_us-w.start_timestamp_us),
            static_cast<int32_t>(w.quality.reasons), static_cast<int32_t>(w.integrity.received_samples),
            static_cast<uint16_t>(w.quality.status) | (static_cast<uint16_t>(status)<<8)));
    };
    const auto& p = result.feature_packet;
    window(BinaryRecordType::PpgWindow,p.ppg_window,static_cast<uint8_t>(p.stress_ppg.status));
    window(BinaryRecordType::EcgWindow,p.ecg_window,static_cast<uint8_t>(p.ecg_af.status));
    window(BinaryRecordType::Spo2Window,p.spo2_window,static_cast<uint8_t>(p.metrics.spo2.estimator_status));
    auto branch = [&](BinaryRecordType type,const BranchResult& b) {
        success &= enqueueRecord(makeRecord(type,timestamp,b.reason_flags,bits(b.score),bits(b.confidence),
            static_cast<uint16_t>(b.status) | (static_cast<uint16_t>(b.label)<<8)));
    };
    branch(BinaryRecordType::SummaryRhythm,result.rhythm);
    branch(BinaryRecordType::SummaryStress,result.stress);
    success &= enqueueRecord(makeRecord(BinaryRecordType::SummaryFeatureCounts,timestamp,
        p.stress_ppg.beat_count,static_cast<int32_t>(p.stress_ppg.raw_interval_count) |
        (static_cast<int32_t>(p.stress_ppg.clean_interval_count)<<16),p.ecg_af.interval_count,config::kFeatureSchemaVersion));
    for (unsigned i=0;i<2;++i) {
        const auto& d=i==0?p.ppg_window.integrity:p.ecg_window.integrity;
        success &= enqueueRecord(makeRecord(i==0?BinaryRecordType::PpgIntegrity:BinaryRecordType::EcgIntegrity,
            timestamp,d.expected_samples,d.received_samples,d.dropped_samples,0));
    }
    return success;
#else
    (void)result;
    return true;
#endif
}

void BinaryLogger::flush(uint8_t max_records) {
#if APP_ENABLE_BINARY_LOG
    BinaryLogRecord record{};
    for (uint8_t index = 0; index < max_records; ++index) {
        if (Serial.availableForWrite() < static_cast<int>(sizeof(record)) || !queue_.pop(record)) {
            break;
        }
        Serial.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
    }
#else
    (void)max_records;
#endif
}

uint32_t BinaryLogger::dropped() const {
    return dropped_;
}

bool BinaryLogger::enqueueRecord(BinaryLogRecord record) {
    record.checksum = checksum(record);
    if (!queue_.push(record)) {
        ++dropped_;
        return false;
    }
    return true;
}

uint16_t BinaryLogger::checksum(const BinaryLogRecord& record) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint16_t crc = 0xFFFFU;
    for (size_t index = 0; index < offsetof(BinaryLogRecord, checksum); ++index) {
        crc ^= static_cast<uint16_t>(bytes[index]) << 8U;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0 ? static_cast<uint16_t>((crc << 1U) ^ 0x1021U)
                                      : static_cast<uint16_t>(crc << 1U);
        }
    }
    return crc;
}

}
