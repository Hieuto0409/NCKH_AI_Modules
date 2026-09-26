#include "features/feature_builder.h"
#include "config/versions.h"
#include "features/ecg_features.h"
#include "features/ppg_features.h"
#include "quality/ppg_sqi.h"
#include "quality/quality_gate.h"
#include <algorithm>
#include <cmath>

namespace ppgfw {
void FeatureBuilder::resetPipeline() {
    measuring_ = false;
    have_ppg_ = have_ecg_ = false;
    ppg_watermark_ = ecg_watermark_ = 0;
    ppg_preprocessor_.reset(); ecg_preprocessor_.reset();
    ppg_peak_detector_.reset(); rpeak_detector_.reset();
}
void FeatureBuilder::reset(uint64_t start_us) {
    resetPipeline(); beginMeasurementWindow(start_us);
}
void FeatureBuilder::beginMeasurementWindow(uint64_t start_us) {
    window_start_us_ = start_us;
    window_end_us_ = start_us + config::kMeasurementWindowMs * 1000ULL;
    measuring_ = true;
    stress_adapter_.reset(start_us); ecg_windows_.reset(start_us);
    spo2_adapter_.reset({config::kPpgSampleRateHz, config::kMax30102RedLedCurrent,
                         config::kMax30102IrLedCurrent, config::kMax30102AdcRangeNa});
    ac_dc_estimator_.reset(); ppg_peak_amplitudes_.reset();
    ppg_integrity_ = {}; ppg_peak_count_ = 0; ppg_contact_samples_ = 0;
    last_spo2_source_us_ = 0;
    // Preserve warmed filters/detectors and transport continuity.
}
bool FeatureBuilder::windowDrained(uint64_t now_us) const {
    // One subsequent sample completes local-maximum detection. A missing sensor
    // cannot stall the UI forever: completeness will reject its branch.
    return (ppg_watermark_ >= window_end_us_ && ecg_watermark_ >= window_end_us_) ||
           now_us >= window_end_us_ + 160000ULL;
}
void FeatureBuilder::consume(const PpgSample& sample) {
    const bool in_window = measuring_ && sample.timestamp_us >= window_start_us_ &&
                           sample.timestamp_us < window_end_us_;
    const bool discontinuity = (sample.flags & (SampleDropoutContext | SampleOverflowContext)) ||
        !(sample.flags & SampleValid) || (have_ppg_ &&
        (sample.seq != last_ppg_sequence_ + 1U ||
         sample.timestamp_us != last_ppg_timestamp_ + config::kPpgSamplePeriodUs));
    if (discontinuity) {
        ppg_preprocessor_.reset(); ppg_peak_detector_.reset();
        if (in_window) {
            ++ppg_integrity_.dropped_samples;
            // A rejected window must not even expose a cross-gap PPI vector.
            stress_adapter_.reset(window_start_us_);
        }
    }
    if (have_ppg_ && sample.timestamp_us <= last_ppg_timestamp_) return;
    have_ppg_ = true;
    last_ppg_sequence_ = sample.seq; last_ppg_timestamp_ = sample.timestamp_us;
    ppg_watermark_ = sample.timestamp_us;
    if (in_window) {
        ++ppg_integrity_.received_samples;
        if (sample.flags & SampleClipping) ++ppg_integrity_.clipping_count;
        if (sample.flags & SampleOverflowContext) ++ppg_integrity_.overflow_count;
        if (sample.flags & SampleContact) ++ppg_contact_samples_;
        spo2_adapter_.push(sample); last_spo2_source_us_ = sample.timestamp_us;
        ac_dc_estimator_.add(sample.red, sample.ir);
    }
    const auto processed = ppg_preprocessor_.process(sample);
    const auto peak = ppg_peak_detector_.update(processed.ir_ac, processed.timestamp_us);
    if (!measuring_ || !peak.detected || peak.timestamp_us < window_start_us_ ||
        peak.timestamp_us >= window_end_us_) return;
    ++ppg_peak_count_; stress_adapter_.addPeak(peak.timestamp_us);
    ppg_peak_amplitudes_.add(peak.amplitude);
}
void FeatureBuilder::consume(const EcgSample& sample) {
    const bool discontinuity = (sample.flags & (SampleDropoutContext | SampleOverflowContext | SampleLeadOff)) ||
        !(sample.flags & SampleValid) || (have_ecg_ &&
        (sample.seq != last_ecg_sequence_ + 1U || sample.timestamp_us <= last_ecg_timestamp_ ||
         sample.timestamp_us - last_ecg_timestamp_ >= 2 * config::kEcgSamplePeriodUs));
    if (discontinuity) {
        ecg_preprocessor_.reset(); rpeak_detector_.reset();
        if (measuring_) ecg_windows_.breakContinuity(sample.timestamp_us);
    }
    if (have_ecg_ && sample.timestamp_us <= last_ecg_timestamp_) return;
    have_ecg_ = true;
    last_ecg_sequence_ = sample.seq; last_ecg_timestamp_ = sample.timestamp_us;
    ecg_watermark_ = sample.timestamp_us;
    const auto processed = ecg_preprocessor_.process(sample);
    const auto peak = rpeak_detector_.update(processed.value, processed.timestamp_us);
    if (!measuring_) return;
    ecg_windows_.addSample(sample, processed.value);
    if (peak.detected && !(sample.flags & SampleLeadOff))
        ecg_windows_.addPeak(peak.timestamp_us, peak.amplitude);
}
FeaturePacket FeatureBuilder::build(uint64_t end_us, const IntegrityDiagnostics&,
                                    const IntegrityDiagnostics&) {
    FeaturePacket packet{};
    packet.schema = FeatureSchema::V3;
    packet.window_start_us = window_start_us_;
    packet.window_end_us = std::min(end_us, window_end_us_);
    auto integrity = ppg_integrity_;
    integrity.expected_samples = config::kPpgSampleRateHz * config::kMeasurementWindowMs / 1000U;
    if (integrity.received_samples < integrity.expected_samples)
        integrity.dropped_samples = std::max(integrity.dropped_samples,
            integrity.expected_samples - integrity.received_samples);
    const auto signal = ac_dc_estimator_.snapshot();
    packet.ppg_quality = PpgSqi::evaluate(
        {integrity, signal, ppg_peak_count_, ppg_contact_samples_ == integrity.received_samples &&
                                          ppg_contact_samples_ > 0});
    packet.ppg_window = {window_start_us_, packet.window_end_us, integrity,
                         packet.ppg_quality, config::kPpgDspConfigVersion};
    packet.stress_ppg = stress_adapter_.build(packet.window_end_us);
    const bool ppg_ok = QualityGate::permitsPpgMetrics(packet.ppg_quality);
    if (!ppg_ok) packet.stress_ppg.status = FeatureVectorStatus::QualityRejected;
    if (ppg_ok && packet.stress_ppg.status == FeatureVectorStatus::Ready) {
        const auto& f = packet.stress_ppg.values;
        auto at = [&](StressPpgFeatureIndex i) { return static_cast<float>(f[static_cast<size_t>(i)]); };
        const float median = at(StressPpgFeatureIndex::MedianPpMs);
        if (packet.stress_ppg.clean_interval_count >= 4 && median > 0)
            packet.metrics.ppg_pulse_rate_bpm = {ValueStatus::Valid, 60000.0F / median};
        packet.metrics.prv = {ValueStatus::Valid, at(StressPpgFeatureIndex::MeanPpMs),
            at(StressPpgFeatureIndex::SdnnMs), at(StressPpgFeatureIndex::RmssdMs),
            at(StressPpgFeatureIndex::Pnn50Pct), packet.stress_ppg.clean_interval_count};
        packet.validity_mask |= 1U;
    } else if (!ppg_ok) {
        packet.metrics.ppg_pulse_rate_bpm.status = ValueStatus::InvalidSignal;
        packet.metrics.prv.status = ValueStatus::InvalidSignal;
    }
    const auto ecg = ecg_windows_.build(packet.window_end_us);
    packet.ecg_window = ecg.metadata; packet.ecg_quality = ecg.metadata.quality;
    packet.ecg_af = ecg.features;
    if (ecg.features.status == FeatureVectorStatus::Ready &&
        QualityGate::permitsEcgMetrics(packet.ecg_quality)) {
        const auto& f = ecg.features.values;
        auto at = [&](EcgAfFeatureIndex i) { return f[static_cast<size_t>(i)]; };
        packet.metrics.ecg_heart_rate_bpm = {ValueStatus::Valid, 60.0F / at(EcgAfFeatureIndex::MeanRr)};
        packet.metrics.hrv = {ValueStatus::Valid, at(EcgAfFeatureIndex::MeanRr)*1000,
            at(EcgAfFeatureIndex::Sdnn)*1000, at(EcgAfFeatureIndex::Rmssd)*1000,
            at(EcgAfFeatureIndex::Pnn50), ecg.features.interval_count};
        packet.validity_mask |= 2U;
    } else if (!QualityGate::permitsEcgMetrics(packet.ecg_quality)) {
        packet.metrics.ecg_heart_rate_bpm.status = ValueStatus::InvalidSignal;
        packet.metrics.hrv.status = ValueStatus::InvalidSignal;
    }
    packet.metrics.spo2 = spo2_adapter_.evaluate(ppg_ok);
    // Latest 400 complete pairs (4 s); conservative full-PPG-window quality gate.
    const uint32_t retained = std::min(spo2_adapter_.modulePairCount(), 400U) * 2U;
    const uint64_t spo2_end = retained ? last_spo2_source_us_ + config::kPpgSamplePeriodUs -
        (spo2_adapter_.sourceSampleCount()%2)*config::kPpgSamplePeriodUs : 0;
    packet.spo2_window = {retained ? spo2_end-retained*config::kPpgSamplePeriodUs : 0,
        spo2_end, {}, packet.ppg_quality, config::kSpo2AlgorithmVersion};
    packet.spo2_window.integrity.received_samples = retained;
    packet.spo2_window.integrity.expected_samples = 800;
    if (packet.metrics.spo2.status == ValueStatus::Valid) packet.validity_mask |= 4U;
    const auto amplitudes = ppg_peak_amplitudes_.snapshot();
    packet.ppg = buildPpgFeatures(signal, packet.metrics, packet.ppg_quality,
        ppg_peak_count_, static_cast<float>(amplitudes.mean), static_cast<float>(std::sqrt(amplitudes.variance)));
    packet.ecg = buildEcgFeatures(ecg.statistics, packet.metrics, ecg.peak_count, ecg.mean_peak_amplitude);
    // No cross-window pulse-arrival estimate: the selected ECG can end earlier.
    return packet;
}
}
