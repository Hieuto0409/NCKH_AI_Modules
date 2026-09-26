#include "features/ecg_window_builder.h"
#include "quality/ecg_sqi.h"
#include "config/versions.h"
#include <algorithm>
namespace ppgfw {
void EcgWindowBuilder::reset(uint64_t start_us) {
    for (size_t i=0; i<candidates_.size(); ++i) {
        candidates_[i] = {};
        candidates_[i].start = start_us + i*kStepUs;
    }
}
void EcgWindowBuilder::addSample(const EcgSample& s, float value) {
    for (auto& c : candidates_) {
        if (s.timestamp_us < c.start || s.timestamp_us >= c.start+kDurationUs) continue;
        ++c.integrity.received_samples;
        if ((s.flags & (SampleDropoutContext | SampleOverflowContext)) ||
            (c.have_sequence && s.seq != c.last_sequence+1U)) {
            ++c.integrity.dropped_samples;
            c.have_peak = false;
        }
        c.last_sequence=s.seq; c.have_sequence=true;
        if (!(s.flags & SampleValid)) ++c.integrity.dropped_samples;
        if (s.flags & SampleClipping) ++c.integrity.clipping_count;
        if (s.flags & SampleLeadOff) { ++c.integrity.lead_off_count; c.have_peak=false; }
        c.signal.add(value);
    }
}
void EcgWindowBuilder::breakContinuity(uint64_t timestamp) {
    for (auto& c : candidates_) {
        if (timestamp >= c.start && timestamp < c.start+kDurationUs) {
            c.have_peak = false;
            ++c.integrity.dropped_samples;
        }
    }
}
void EcgWindowBuilder::addPeak(uint64_t timestamp, float amplitude) {
    for (auto& c : candidates_) {
        if (timestamp < c.start || timestamp >= c.start+kDurationUs) continue;
        if (c.have_peak && timestamp <= c.last_peak) continue;
        ++c.peaks; c.amplitudes.add(amplitude);
        if (c.have_peak) {
            if (c.count < c.rr.size()) c.rr[c.count++] = float(timestamp-c.last_peak)/1000.0F;
            else ++c.integrity.dropped_samples;
        }
        c.last_peak=timestamp; c.have_peak=true;
    }
}
EcgWindowResult EcgWindowBuilder::build(uint64_t end_us) const {
    EcgWindowResult selected{};
    bool have_selected=false, selected_good=false;
    for (const auto& c : candidates_) {
        if (end_us < c.start+kDurationUs) continue;
        auto integrity=c.integrity;
        integrity.expected_samples=static_cast<uint32_t>(kDurationUs/config::kEcgSamplePeriodUs);
        // Counts drive the existing completeness gate. Actual timestamps can
        // shift a boundary sample; only observed transport gaps count as drops.
        const auto stats=c.signal.snapshot();
        auto quality=EcgSqi::evaluate({integrity,stats,c.peaks});
        auto features=EcgAfFeatureAdapter::build(c.rr.data(),c.count);
        const bool good=quality.status==QualityStatus::Good && features.status==FeatureVectorStatus::Ready;
        // Same count-based tie policy as the pinned offline bridge: first wins.
        if (!have_selected || (good && !selected_good) ||
            (good==selected_good && features.interval_count>selected.features.interval_count)) {
            if (quality.status != QualityStatus::Good) features.status=FeatureVectorStatus::QualityRejected;
            selected.metadata={c.start,c.start+kDurationUs,integrity,quality,config::kEcgDspConfigVersion};
            selected.features=features; selected.statistics=stats; selected.peak_count=c.peaks;
            selected.mean_peak_amplitude=static_cast<float>(c.amplitudes.snapshot().mean);
            selected_good=good; have_selected=true;
        }
    }
    return selected;
}
}
