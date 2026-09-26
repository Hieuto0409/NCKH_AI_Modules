#include "drivers/max30102_fifo.h"
#include <algorithm>
namespace ppgfw {
bool Max30102Fifo::recover(Max30102Bus& bus) {
    uint8_t mode{};
    if (!bus.read(0x09, &mode, 1) || !bus.write(0x09, mode | 0x80)) return false;
    const bool cleared = bus.write(0x04, 0) && bus.write(0x05, 0) && bus.write(0x06, 0);
    // A_FULL is latched independently of the FIFO pointers. Clear it while
    // stopped, otherwise a recovered empty FIFO can look like 32 unread samples.
    uint8_t status{};
    const bool status_cleared = bus.read(0x00, &status, 1);
    const bool resumed = bus.write(0x09, mode & 0x7f);
    return cleared && status_cleared && resumed;
}
FifoDrainResult Max30102Fifo::drain(Max30102Bus& bus, PpgFifoSample* output,
                                   size_t capacity, bool continuity_unknown) {
    FifoDrainResult result{};
    if (!output || capacity == 0) return result;
    auto fault = [&](uint32_t lost, bool io) {
        result.count = 0;
        result.lost_samples = std::max(1U, lost);
        result.discontinuity = true;
        result.io_error = io;
        recovery_pending_ = !recover(bus);
        result.io_error |= recovery_pending_;
        return result;
    };
    if (recovery_pending_ || continuity_unknown) return fault(1, recovery_pending_);
    uint8_t status{}, pointers[3]{};
    if (!bus.read(0x00, &status, 1) || !bus.read(0x04, pointers, 3)) return fault(1, true);
    if (status & 0x01) return fault(1, true);
    const unsigned overflow = pointers[1] & 31;
    if (overflow) return fault(overflow + 32U, false);
    size_t unread = (pointers[0] - pointers[2]) & 31;
    if (unread == 0 && (status & 0x80)) unread = 32;
    if (unread > capacity) return fault(static_cast<uint32_t>(unread), false);
    while (result.count < unread) {
        const size_t samples = std::min<size_t>(5, unread - result.count);
        uint8_t bytes[30]{};
        if (!bus.read(0x07, bytes, samples * 6)) return fault(static_cast<uint32_t>(unread), true);
        for (size_t i = 0; i < samples; ++i) {
            const uint8_t* p = bytes + i * 6;
            const uint32_t red = ((uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2]) & 0x3ffff;
            const uint32_t ir = ((uint32_t(p[3]) << 16) | (uint32_t(p[4]) << 8) | p[5]) & 0x3ffff;
            output[result.count++] = {red, ir};
        }
    }
    return result;
}
}
