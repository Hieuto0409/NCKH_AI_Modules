#include "quality/ecg_sqi.h"

#include "acquisition/sample_integrity.h"
#include "config/thresholds.h"
#include "config/versions.h"

#include <algorithm>
#include <cmath>

namespace ppgfw {

QualitySummary EcgSqi::evaluate(const EcgQualityInput& input) {
    QualitySummary quality{};
    quality.config_version = config::kQualityConfigVersion;
    quality.completeness = SampleIntegrity::completeness(input.integrity);
    quality.clipping_fraction = input.integrity.received_samples > 0
                                    ? static_cast<float>(input.integrity.clipping_count) /
                                          input.integrity.received_samples
                                    : 1.0F;
    quality.variation = static_cast<float>(std::sqrt(input.signal.variance));

    if (quality.completeness < config::candidate::kMinimumCompleteness) {
        quality.reasons |= QualityIncomplete;
    }
    if (input.integrity.queue_drop_count > 0 || input.integrity.dropped_samples > 0) {
        quality.reasons |= QualityOverflow;
    }
    if (quality.clipping_fraction > config::candidate::kMaximumClipFraction) {
        quality.reasons |= QualityClipping;
    }
    if (input.integrity.lead_off_count > 0) {
        quality.reasons |= QualityLeadOff;
    }
    if (quality.variation < config::candidate::kMinimumEcgStandardDeviation) {
        quality.reasons |= QualityLowVariation;
    }
    if (input.peak_count < 3) {
        quality.reasons |= QualityImplausiblePeaks;
    }

    const float completeness_score = std::clamp(quality.completeness, 0.0F, 1.0F);
    const float clipping_score = std::clamp(1.0F - quality.clipping_fraction * 20.0F, 0.0F, 1.0F);
    const float variation_score = std::clamp(quality.variation /
                                                  (config::candidate::kMinimumEcgStandardDeviation * 4.0F),
                                              0.0F, 1.0F);
    const float lead_score = input.integrity.lead_off_count == 0 ? 1.0F : 0.0F;
    quality.score = (completeness_score + clipping_score + variation_score + lead_score) * 0.25F;
    quality.status = quality.reasons == QualityNone ? QualityStatus::Good : QualityStatus::Poor;
    return quality;
}

}

