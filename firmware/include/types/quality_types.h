#pragma once

#include <cstdint>

namespace ppgfw {

enum class ValueStatus : uint8_t {
    Valid,
    InvalidSignal,
    NotAvailable,
    Error
};

enum class QualityStatus : uint8_t {
    Unknown,
    Good,
    Poor
};

enum QualityReason : uint32_t {
    QualityNone = 0,
    QualityIncomplete = 1U << 0U,
    QualityOverflow = 1U << 1U,
    QualityClipping = 1U << 2U,
    QualityNoContact = 1U << 3U,
    QualityLowPerfusion = 1U << 4U,
    QualityLowCorrelation = 1U << 5U,
    QualityLeadOff = 1U << 6U,
    QualityLowVariation = 1U << 7U,
    QualityImplausiblePeaks = 1U << 8U
};

struct QualitySummary {
    QualityStatus status{QualityStatus::Unknown};
    float score{};
    float completeness{};
    float clipping_fraction{};
    float perfusion_index{};
    float correlation{};
    float variation{};
    uint32_t reasons{QualityNone};
    uint16_t config_version{};
};

}

