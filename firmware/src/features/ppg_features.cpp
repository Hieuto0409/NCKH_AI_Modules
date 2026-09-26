#include "features/ppg_features.h"

#include <cmath>
#include <limits>

namespace ppgfw {

namespace {
void set(PpgFeatureSet& output, PpgFeatureIndex index, float value, bool valid = true) {
    const size_t position = static_cast<size_t>(index);
    output.values[position] = value;
    if (valid && std::isfinite(value)) {
        output.validity_mask |= 1UL << position;
    }
}
}

PpgFeatureSet buildPpgFeatures(const AcDcSnapshot& signal, const MetricSnapshot& metrics,
                               const QualitySummary& quality, uint16_t peak_count,
                               float mean_peak_amplitude, float peak_amplitude_variation) {
    PpgFeatureSet output{};
    output.values.fill(std::numeric_limits<float>::quiet_NaN());
    set(output, PpgFeatureIndex::RedMean, static_cast<float>(signal.red.mean));
    set(output, PpgFeatureIndex::RedStandardDeviation, static_cast<float>(std::sqrt(signal.red.variance)));
    set(output, PpgFeatureIndex::RedMinimum, static_cast<float>(signal.red.minimum));
    set(output, PpgFeatureIndex::RedMaximum, static_cast<float>(signal.red.maximum));
    set(output, PpgFeatureIndex::IrMean, static_cast<float>(signal.ir.mean));
    set(output, PpgFeatureIndex::IrStandardDeviation, static_cast<float>(std::sqrt(signal.ir.variance)));
    set(output, PpgFeatureIndex::IrMinimum, static_cast<float>(signal.ir.minimum));
    set(output, PpgFeatureIndex::IrMaximum, static_cast<float>(signal.ir.maximum));
    set(output, PpgFeatureIndex::RedAc, signal.red_ac);
    set(output, PpgFeatureIndex::RedDc, signal.red_dc);
    set(output, PpgFeatureIndex::IrAc, signal.ir_ac);
    set(output, PpgFeatureIndex::IrDc, signal.ir_dc);
    set(output, PpgFeatureIndex::RedPerfusionIndex,
        signal.red_dc > 0.0F ? signal.red_ac / signal.red_dc : kUnavailableValue);
    set(output, PpgFeatureIndex::IrPerfusionIndex,
        signal.ir_dc > 0.0F ? signal.ir_ac / signal.ir_dc : kUnavailableValue);
    set(output, PpgFeatureIndex::RedIrCorrelation, signal.correlation);
    set(output, PpgFeatureIndex::PulseRate, metrics.ppg_pulse_rate_bpm.value,
        metrics.ppg_pulse_rate_bpm.status == ValueStatus::Valid);
    set(output, PpgFeatureIndex::MeanPpi, metrics.prv.mean_interval_ms,
        metrics.prv.status == ValueStatus::Valid);
    set(output, PpgFeatureIndex::SdnnPpi, metrics.prv.sdnn_ms, metrics.prv.status == ValueStatus::Valid);
    set(output, PpgFeatureIndex::RmssdPpi, metrics.prv.rmssd_ms, metrics.prv.status == ValueStatus::Valid);
    set(output, PpgFeatureIndex::Pnn50Ppi, metrics.prv.pnn50_percent,
        metrics.prv.status == ValueStatus::Valid);
    set(output, PpgFeatureIndex::PeakCount, peak_count);
    set(output, PpgFeatureIndex::MeanPulseAmplitude, mean_peak_amplitude);
    set(output, PpgFeatureIndex::PulseAmplitudeVariation, peak_amplitude_variation);
    set(output, PpgFeatureIndex::SignalRangeRed,
        static_cast<float>(signal.red.maximum - signal.red.minimum));
    set(output, PpgFeatureIndex::SignalRangeIr,
        static_cast<float>(signal.ir.maximum - signal.ir.minimum));
    set(output, PpgFeatureIndex::Spo2Ratio, metrics.spo2.ratio,
        std::isfinite(metrics.spo2.ratio));
    set(output, PpgFeatureIndex::SampleCompleteness, quality.completeness);
    set(output, PpgFeatureIndex::ClippingFraction, quality.clipping_fraction);
    return output;
}

}

