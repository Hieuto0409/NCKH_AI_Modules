#pragma once
#include "types/raw_samples.h"
#include <cstddef>
#include <cstdint>
namespace ppgfw {
class Max30102Bus {
public:
    virtual ~Max30102Bus() = default;
    virtual bool read(uint8_t reg, uint8_t* bytes, size_t count) = 0;
    virtual bool write(uint8_t reg, uint8_t value) = 0;
};
struct FifoDrainResult {
    size_t count{};
    uint32_t lost_samples{}; // lower bound when transport/overflow is ambiguous
    bool discontinuity{};
    bool io_error{};
};
// Requires rollover disabled and A_FULL threshold zero (32 unread samples).
class Max30102Fifo {
public:
    FifoDrainResult drain(Max30102Bus& bus, PpgFifoSample* output, size_t capacity,
                          bool continuity_unknown = false);
private:
    bool recover(Max30102Bus& bus);
    bool recovery_pending_{};
};
}
