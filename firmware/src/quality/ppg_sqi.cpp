#include "quality/ppg_sqi.h"

#include "acquisition/sample_integrity.h"
#include "config/thresholds.h"
#include "config/versions.h"

#include <algorithm>
#include <cmath>

namespace ppgfw {

QualitySummary PpgSqi::evaluate(const PpgQualityInput& input) {
    QualitySummary quality{};
    quality.config_version = config::kQualityConfigVersion;
    quality.completeness = SampleIntegrity::completeness(input.integrity);
    quality.clipping_fraction = input.integrity.received_samples > 0
                                    ? static_cast<float>(input.integrity.clipping_count) /
                                          input.integrity.received_samples
                                    : 1.0F;
    quality.perfusion_index = input.signal.ir_dc > 0.0F
                                  ? input.signal.ir_ac / input.signal.ir_dc
                                  : 0.0F;
    quality.correlation = input.signal.correlation;
    quality.variation = input.signal.ir_ac;

    if (quality.completeness < config::candidate::kMinimumCompleteness ||
        input.integrity.dropped_samples > 0) {
        quality.reasons |= QualityIncomplete;
    }
    if (input.integrity.overflow_count > 0 || input.integrity.queue_drop_count > 0) {
        quality.reasons |= QualityOverflow;
    }
    if (quality.clipping_fraction > config::candidate::kMaximumClipFraction) {
        quality.reasons |= QualityClipping;
    }
    if (!input.contact_seen) {
        quality.reasons |= QualityNoContact;
    }
    if (quality.perfusion_index < config::candidate::kMinimumPpgPerfusionIndex) {
        quality.reasons |= QualityLowPerfusion;
    }
    if (std::fabs(quality.correlation) < config::candidate::kMinimumRedIrCorrelation) {
        quality.reasons |= QualityLowCorrelation;
    }
    if (input.peak_count < 3) {
        quality.reasons |= QualityImplausiblePeaks;
    }

    const float completeness_score = std::clamp(quality.completeness, 0.0F, 1.0F);
    const float clipping_score = std::clamp(1.0F - quality.clipping_fraction * 20.0F, 0.0F, 1.0F);
    const float perfusion_score = std::clamp(quality.perfusion_index /
                                                  (config::candidate::kMinimumPpgPerfusionIndex * 3.0F),
                                              0.0F, 1.0F);
    const float correlation_score = std::clamp(std::fabs(quality.correlation), 0.0F, 1.0F);
    quality.score = (completeness_score + clipping_score + perfusion_score + correlation_score) * 0.25F;
    quality.status = quality.reasons == QualityNone ? QualityStatus::Good : QualityStatus::Poor;
    return quality;
}

}
