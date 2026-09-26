#pragma once

#include <cstdint>

namespace ppgfw {

enum SampleFlag : uint16_t {
    SampleValid = 1U << 0U,
    SampleClipping = 1U << 1U,
    SampleOverflowContext = 1U << 2U,
    SampleDropoutContext = 1U << 3U,
    SampleLeadOff = 1U << 4U,
    SampleContact = 1U << 5U
};

struct PpgSample {
    uint64_t timestamp_us{};
    uint32_t seq{};
    uint32_t red{};
    uint32_t ir{};
    uint16_t flags{};
};

struct EcgSample {
    uint64_t timestamp_us{};
    uint32_t seq{};
    int16_t raw{};
    uint16_t flags{};
};

struct PpgFifoSample {
    uint32_t red{};
    uint32_t ir{};
};

}

