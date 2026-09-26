#pragma once

#include "config/sampling.h"
#include "types/raw_samples.h"
#include "types/result_types.h"
#include "utils/spsc_ring_buffer.h"

#include <cstdint>

namespace ppgfw {

enum class BinaryRecordType : uint8_t {
    Ppg = 1,
    Ecg = 2,
    Session = 16,
    Config = 17,
    HardwareConfig = 18,
    AiProvenance = 19,
    SummaryQuality = 32,
    SummaryMetrics = 33,
    SummarySpo2 = 34,
    SummaryDecision = 35,
    SummaryModelVersions = 36,
    SummaryInferenceLatency = 37,
    PpgWindow = 38,
    EcgWindow = 39,
    Spo2Window = 40,
    SummaryRhythm = 41,
    SummaryStress = 42,
    SummaryFeatureCounts = 43,
    PpgIntegrity = 44,
    EcgIntegrity = 45
};

#pragma pack(push, 1)
struct BinaryLogRecord {
    uint16_t sync{0xA55A};
    uint8_t schema{};
    BinaryRecordType type{BinaryRecordType::Ppg};
    uint64_t timestamp_us{};
    uint32_t sequence{};
    int32_t value_a{};
    int32_t value_b{};
    uint16_t flags{};
    uint16_t checksum{};
};
#pragma pack(pop)

static_assert(sizeof(BinaryLogRecord) == 28, "Binary log schema size changed");

class BinaryLogger {
public:
    void begin(uint64_t session_id, uint64_t device_id);
    bool enqueue(const PpgSample& sample);
    bool enqueue(const EcgSample& sample);
    bool enqueue(const ResultSnapshot& result);
    void flush(uint8_t max_records);
    uint32_t dropped() const;

private:
    bool enqueueRecord(BinaryLogRecord record);
    static uint16_t checksum(const BinaryLogRecord& record);
    SpscRingBuffer<BinaryLogRecord, config::kLoggerQueueCapacity> queue_{};
    uint32_t dropped_{};
};

}
