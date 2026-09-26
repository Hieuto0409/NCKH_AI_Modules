#include "acquisition/sample_integrity.h"

#include "config/thresholds.h"

namespace ppgfw {

uint16_t SampleIntegrity::ppgFlags(const PpgFifoSample& sample, bool overflow_context) {
    uint16_t flags = SampleValid;
    if (sample.red <= config::candidate::kPpgRawClipLow ||
        sample.ir <= config::candidate::kPpgRawClipLow ||
        sample.red >= config::candidate::kPpgRawClipHigh ||
        sample.ir >= config::candidate::kPpgRawClipHigh) {
        flags |= SampleClipping;
    }
    if (sample.ir >= config::candidate::kFingerIrMinimum) {
        flags |= SampleContact;
    }
    if (overflow_context) {
        flags |= SampleOverflowContext;
    }
    return flags;
}

uint16_t SampleIntegrity::ecgFlags(int16_t raw, bool lead_off, bool dropout_context) {
    uint16_t flags = SampleValid;
    if (raw <= config::candidate::kEcgRawClipLow || raw >= config::candidate::kEcgRawClipHigh) {
        flags |= SampleClipping;
    }
    if (lead_off) {
        flags |= SampleLeadOff;
    }
    if (dropout_context) {
        flags |= SampleDropoutContext;
    }
    return flags;
}

float SampleIntegrity::completeness(const IntegrityDiagnostics& diagnostics) {
    if (diagnostics.expected_samples == 0) {
        return 0.0F;
    }
    const float ratio = static_cast<float>(diagnostics.received_samples) /
                        static_cast<float>(diagnostics.expected_samples);
    return ratio > 1.0F ? 1.0F : ratio;
}

}

