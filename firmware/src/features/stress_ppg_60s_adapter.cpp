#include "features/stress_ppg_60s_adapter.h"

#include "config/model_contracts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ppgfw {

namespace {

double mean(const double* values, size_t count) {
    double sum = 0.0;
    for (size_t index = 0; index < count; ++index) {
        sum += values[index];
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

double sampleStandardDeviation(const double* values, size_t count, double average) {
    if (count < 2) {
        return 0.0;
    }
    double sum = 0.0;
    for (size_t index = 0; index < count; ++index) {
        const double delta = values[index] - average;
        sum += delta * delta;
    }
    return std::sqrt(sum / static_cast<double>(count - 1U));
}

double median(std::array<double, config::kIntervalCapacity> values, size_t count) {
    std::sort(values.begin(), values.begin() + count);
    return (count & 1U) != 0U
               ? values[count / 2U]
               : 0.5 * (values[count / 2U - 1U] + values[count / 2U]);
}

void set(StressPpgFeatureVector& output, StressPpgFeatureIndex index, double value) {
    output.values[static_cast<size_t>(index)] = value;
}

}

void StressPpg60sAdapter::reset(uint64_t window_start_us) {
    peak_count_ = 0;
    window_start_us_ = window_start_us;
}

void StressPpg60sAdapter::addPeak(uint64_t timestamp_us) {
    if (peak_count_ > 0 && timestamp_us <= peak_timestamps_us_[peak_count_ - 1U]) {
        return;
    }
    if (peak_count_ == peak_timestamps_us_.size()) {
        std::move(peak_timestamps_us_.begin() + 1, peak_timestamps_us_.end(),
                  peak_timestamps_us_.begin());
        --peak_count_;
    }
    peak_timestamps_us_[peak_count_++] = timestamp_us;
}

StressPpgFeatureVector StressPpg60sAdapter::build(uint64_t window_end_us) const {
    StressPpgFeatureVector output{};
    output.window_end_us = window_end_us;
    if (window_end_us < window_start_us_ ||
        window_end_us - window_start_us_ < config::model::kStressWindowUs) {
        output.window_start_us = window_start_us_;
        return output;
    }

    output.window_start_us = window_end_us - config::model::kStressWindowUs;
    std::array<uint64_t, config::kIntervalCapacity> peaks{};
    size_t beat_count = 0;
    for (size_t index = 0; index < peak_count_; ++index) {
        const uint64_t timestamp = peak_timestamps_us_[index];
        if (timestamp >= output.window_start_us && timestamp < window_end_us) {
            peaks[beat_count++] = timestamp;
        }
    }
    output.beat_count = static_cast<uint16_t>(beat_count);
    if (beat_count < config::model::kStressMinimumBeatCount) {
        output.status = FeatureVectorStatus::QualityRejected;
        return output;
    }

    std::array<double, config::kIntervalCapacity> raw_ppi_ms{};
    const size_t raw_count = beat_count - 1U;
    output.raw_interval_count = static_cast<uint16_t>(raw_count);
    for (size_t index = 0; index < raw_count; ++index) {
        raw_ppi_ms[index] = static_cast<double>(peaks[index + 1U] - peaks[index]) / 1000.0;
    }

    std::array<double, config::kIntervalCapacity> physiological{};
    size_t physiological_count = 0;
    for (size_t index = 0; index < raw_count; ++index) {
        const double value = raw_ppi_ms[index];
        if (value >= config::model::kStressMinimumPpiMs &&
            value <= config::model::kStressMaximumPpiMs) {
            physiological[physiological_count++] = value;
        }
    }
    if (physiological_count == 0) {
        output.status = FeatureVectorStatus::QualityRejected;
        return output;
    }

    const double physiological_median = median(physiological, physiological_count);
    const double lower = physiological_median * (1.0 - config::model::kStressMedianTolerance);
    const double upper = physiological_median * (1.0 + config::model::kStressMedianTolerance);
    std::array<double, config::kIntervalCapacity> clean_ppi_ms{};
    size_t clean_count = 0;
    for (size_t index = 0; index < raw_count; ++index) {
        const double value = raw_ppi_ms[index];
        if (value >= config::model::kStressMinimumPpiMs &&
            value <= config::model::kStressMaximumPpiMs && value >= lower && value <= upper) {
            clean_ppi_ms[clean_count++] = value;
        }
    }
    output.clean_interval_count = static_cast<uint16_t>(clean_count);
    const double valid_ratio = raw_count > 0
                                   ? static_cast<double>(clean_count) / static_cast<double>(raw_count)
                                   : 0.0;
    if (clean_count < 3 || valid_ratio < config::model::kStressMinimumValidPpiRatio) {
        output.status = FeatureVectorStatus::QualityRejected;
        return output;
    }

    std::array<double, config::kIntervalCapacity> heart_rate_bpm{};
    for (size_t index = 0; index < clean_count; ++index) {
        heart_rate_bpm[index] = 60000.0 / clean_ppi_ms[index];
    }
    const double mean_hr = mean(heart_rate_bpm.data(), clean_count);
    const double mean_pp = mean(clean_ppi_ms.data(), clean_count);
    const double sdnn = sampleStandardDeviation(clean_ppi_ms.data(), clean_count, mean_pp);

    std::array<double, config::kIntervalCapacity> differences_ms{};
    const size_t difference_count = clean_count - 1U;
    double squared_difference_sum = 0.0;
    size_t nn20 = 0;
    size_t nn50 = 0;
    for (size_t index = 0; index < difference_count; ++index) {
        const double difference = clean_ppi_ms[index + 1U] - clean_ppi_ms[index];
        differences_ms[index] = difference;
        squared_difference_sum += difference * difference;
        if (std::fabs(difference) > 20.0) {
            ++nn20;
        }
        if (std::fabs(difference) > 50.0) {
            ++nn50;
        }
    }
    if (difference_count < 2) {
        output.status = FeatureVectorStatus::QualityRejected;
        return output;
    }
    const double mean_difference = mean(differences_ms.data(), difference_count);

    set(output, StressPpgFeatureIndex::MeanHrBpm, mean_hr);
    set(output, StressPpgFeatureIndex::StdHrBpm,
        sampleStandardDeviation(heart_rate_bpm.data(), clean_count, mean_hr));
    set(output, StressPpgFeatureIndex::MinHrBpm,
        *std::min_element(heart_rate_bpm.begin(), heart_rate_bpm.begin() + clean_count));
    set(output, StressPpgFeatureIndex::MaxHrBpm,
        *std::max_element(heart_rate_bpm.begin(), heart_rate_bpm.begin() + clean_count));
    set(output, StressPpgFeatureIndex::MeanPpMs, mean_pp);
    set(output, StressPpgFeatureIndex::MedianPpMs, median(clean_ppi_ms, clean_count));
    set(output, StressPpgFeatureIndex::SdnnMs, sdnn);
    set(output, StressPpgFeatureIndex::RmssdMs,
        std::sqrt(squared_difference_sum / static_cast<double>(difference_count)));
    set(output, StressPpgFeatureIndex::SdsdMs,
        sampleStandardDeviation(differences_ms.data(), difference_count, mean_difference));
    set(output, StressPpgFeatureIndex::Pnn20Pct,
        100.0 * static_cast<double>(nn20) / static_cast<double>(difference_count));
    set(output, StressPpgFeatureIndex::Pnn50Pct,
        100.0 * static_cast<double>(nn50) / static_cast<double>(difference_count));
    set(output, StressPpgFeatureIndex::Cvnn, mean_pp > 0.0 ? sdnn / mean_pp : 0.0);
    set(output, StressPpgFeatureIndex::BeatCount, static_cast<double>(beat_count));
    set(output, StressPpgFeatureIndex::ValidRrRatio, valid_ratio);

    for (double value : output.values) {
        if (!std::isfinite(value)) {
            output.status = FeatureVectorStatus::InvalidInput;
            return output;
        }
    }
    output.status = FeatureVectorStatus::Ready;
    return output;
}

}
