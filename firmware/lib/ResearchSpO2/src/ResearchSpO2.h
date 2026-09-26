#pragma once
#include <cstdint>
#include <cstddef>
namespace research_spo2 {
enum class Status { Ok, NeedData, InvalidInput, ContactLost, Clipped,
                    QualityRejected, AlgorithmRejected };
struct Result {
    Status status = Status::NeedData;
    int32_t percent = -1;
    int32_t heartRate = -1;
    bool heartRateValid = false;
    bool valid() const { return status == Status::Ok; }
};
struct Config {
    uint32_t minIr = 10000; // Project-specific contact heuristic, NOT clinical SQI.
};
// Exactly 100 synchronized RAW RED/IR pairs at 25 Hz, oldest first.
// Do not call concurrently: the adapted Maxim backend has static workspace.
Result calculate25(const uint32_t* ir, const uint32_t* red, std::size_t count,
                   bool externalQualityOk, Config config = Config{});
class Stream100 {
public:
    explicit Stream100(Config config = Config{}) : config_(config) { reset(); }
    void reset(); // Call at session start, FIFO overflow, gap, rate/LED changes.
    void push(uint32_t ir, uint32_t red); // Exactly one call per RAW pair at 100 Hz.
    // Evaluates latest data. Caller must supply current, independent quality result.
    Result evaluate(bool externalQualityOk);
private:
    Config config_;
    uint32_t ir_[100], red_[100];
    uint8_t faults_[400];
    uint16_t write_, count_, faultWrite_, rawCount_;
    uint8_t phase_;
};
const char* statusText(Status status);
} // namespace research_spo2
