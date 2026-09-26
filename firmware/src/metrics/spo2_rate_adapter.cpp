#include "metrics/spo2_rate_adapter.h"

#include "config/model_contracts.h"
#include "config/sampling.h"
#include "config/versions.h"

#include <limits>

namespace ppgfw {

namespace {

Spo2EstimatorStatus mapStatus(research_spo2::Status status) {
    switch (status) {
        case research_spo2::Status::Ok: return Spo2EstimatorStatus::Ok;
        case research_spo2::Status::NeedData: return Spo2EstimatorStatus::NeedData;
        case research_spo2::Status::InvalidInput: return Spo2EstimatorStatus::InvalidInput;
        case research_spo2::Status::ContactLost: return Spo2EstimatorStatus::ContactLost;
        case research_spo2::Status::Clipped: return Spo2EstimatorStatus::Clipped;
        case research_spo2::Status::QualityRejected: return Spo2EstimatorStatus::QualityRejected;
        case research_spo2::Status::AlgorithmRejected: return Spo2EstimatorStatus::AlgorithmRejected;
    }
    return Spo2EstimatorStatus::InvalidInput;
}

}

void Spo2RateAdapter200To100::reset(const Spo2InputConfig& config) {
    config_ = config;
    config_initialized_ = true;
    reset_count_ = 0;
    input_rejected_ = config.sample_rate_hz != config::kPpgSampleRateHz;
    resetStream(false);
}

void Spo2RateAdapter200To100::updateConfig(const Spo2InputConfig& config) {
    if (!config_initialized_) {
        reset(config);
        return;
    }
    if (!sameConfig(config)) {
        config_ = config;
        input_rejected_ = config.sample_rate_hz != config::kPpgSampleRateHz;
        resetStream(true);
    }
}

void Spo2RateAdapter200To100::push(const PpgSample& sample) {
    if (input_rejected_) return;
    if (have_previous_ && (sample.timestamp_us <= last_timestamp_us_ || sample.seq == last_sequence_)) {
        resetStream(true);
        return; // never reuse the invalid/duplicate sample
    }
    const bool flagged_discontinuity =
        (sample.flags & (SampleOverflowContext | SampleDropoutContext)) != 0;
    const bool sequence_gap = have_previous_ && sample.seq != last_sequence_ + 1U;
    const bool timestamp_gap = have_previous_ &&
                               sample.timestamp_us != last_timestamp_us_ + config::kPpgSamplePeriodUs;
    if (flagged_discontinuity || sequence_gap || timestamp_gap) {
        resetStream(true);
    }

    ++source_sample_count_;
    if (!have_pending_) {
        pending_ = sample;
        have_pending_ = true;
    } else {
        const uint32_t red = static_cast<uint32_t>(
            (static_cast<uint64_t>(pending_.red) + sample.red + 1ULL) / 2ULL);
        const uint32_t ir = static_cast<uint32_t>(
            (static_cast<uint64_t>(pending_.ir) + sample.ir + 1ULL) / 2ULL);
        stream_.push(ir, red);
        ++module_pair_count_;
        have_pending_ = false;
    }
    last_timestamp_us_ = sample.timestamp_us;
    last_sequence_ = sample.seq;
    have_previous_ = true;
}

Spo2Result Spo2RateAdapter200To100::evaluate(bool external_quality_ok) {
    Spo2Result output{};
    output.algorithm_version = config::kSpo2AlgorithmVersion;
    output.calibration_version = config::kSpo2CalibrationVersion;
    output.source_sample_count = source_sample_count_;
    output.module_pair_count = module_pair_count_;
    output.adapter_reset_count = reset_count_;
    output.clinically_validated = false;

    if (input_rejected_) {
        output.status = ValueStatus::Error;
        output.estimator_status = Spo2EstimatorStatus::InvalidInput;
        return output;
    }
    const research_spo2::Result result = stream_.evaluate(external_quality_ok);
    output.estimator_status = mapStatus(result.status);
    if (result.status == research_spo2::Status::Ok) {
        output.status = ValueStatus::Valid;
        output.percent = static_cast<float>(result.percent);
    } else if (result.status == research_spo2::Status::NeedData) {
        output.status = ValueStatus::NotAvailable;
    } else if (result.status == research_spo2::Status::InvalidInput) {
        output.status = ValueStatus::Error;
    } else {
        output.status = ValueStatus::InvalidSignal;
    }
    return output;
}

uint32_t Spo2RateAdapter200To100::sourceSampleCount() const {
    return source_sample_count_;
}

uint32_t Spo2RateAdapter200To100::modulePairCount() const {
    return module_pair_count_;
}

uint32_t Spo2RateAdapter200To100::resetCount() const {
    return reset_count_;
}

void Spo2RateAdapter200To100::resetStream(bool count_reset) {
    stream_.reset();
    source_sample_count_ = 0;
    module_pair_count_ = 0;
    have_pending_ = false;
    have_previous_ = false;
    last_timestamp_us_ = 0;
    last_sequence_ = 0;
    if (count_reset) {
        ++reset_count_;
    }
}

bool Spo2RateAdapter200To100::sameConfig(const Spo2InputConfig& config) const {
    return config.sample_rate_hz == config_.sample_rate_hz &&
           config.red_led_current == config_.red_led_current &&
           config.ir_led_current == config_.ir_led_current &&
           config.adc_range_na == config_.adc_range_na;
}

}
