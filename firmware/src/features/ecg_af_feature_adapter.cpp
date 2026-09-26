#include "features/ecg_af_feature_adapter.h"

#include "config/model_contracts.h"
#include "config/sampling.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ppgfw {

namespace {

double percentile(const std::array<double, config::kIntervalCapacity>& sorted,
                  size_t count, double fraction) {
    const double position = static_cast<double>(count - 1U) * fraction;
    const size_t lower = static_cast<size_t>(position);
    const size_t upper = std::min(lower + 1U, count - 1U);
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

void set(EcgAfFeatureVector& output, EcgAfFeatureIndex index, double value) {
    output.values[static_cast<size_t>(index)] = static_cast<float>(value);
}

}

EcgAfFeatureVector EcgAfFeatureAdapter::build(const float* intervals_ms, size_t count) {
    EcgAfFeatureVector output{};
    if (intervals_ms == nullptr && count > 0) {
        output.status = FeatureVectorStatus::InvalidInput;
        return output;
    }

    std::array<double, config::kIntervalCapacity> rr_seconds{};
    size_t valid_count = 0;
    for (size_t index = 0; index < count && valid_count < rr_seconds.size(); ++index) {
        const double value_ms = intervals_ms[index];
        if (std::isfinite(value_ms) && value_ms >= config::model::kEcgAfMinimumRrMs &&
            value_ms <= config::model::kEcgAfMaximumRrMs) {
            rr_seconds[valid_count++] = value_ms / 1000.0;
        }
    }
    output.interval_count = static_cast<uint16_t>(valid_count);
    if (valid_count < config::model::kEcgAfMinimumRrCount) {
        return output;
    }

    double sum = 0.0;
    for (size_t index = 0; index < valid_count; ++index) {
        sum += rr_seconds[index];
    }
    const double mean_rr = sum / static_cast<double>(valid_count);
    double squared_deviation_sum = 0.0;
    for (size_t index = 0; index < valid_count; ++index) {
        const double delta = rr_seconds[index] - mean_rr;
        squared_deviation_sum += delta * delta;
    }
    const double sdnn = std::sqrt(squared_deviation_sum / static_cast<double>(valid_count - 1U));

    double squared_difference_sum = 0.0;
    size_t nn50 = 0;
    for (size_t index = 0; index + 1U < valid_count; ++index) {
        const double difference = rr_seconds[index + 1U] - rr_seconds[index];
        squared_difference_sum += difference * difference;
        if (std::fabs(difference) > 0.050) {
            ++nn50;
        }
    }
    const size_t difference_count = valid_count - 1U;
    const double rmssd = std::sqrt(squared_difference_sum /
                                   static_cast<double>(difference_count));

    std::array<double, config::kIntervalCapacity> sorted = rr_seconds;
    std::sort(sorted.begin(), sorted.begin() + valid_count);
    const double median = percentile(sorted, valid_count, 0.50);
    const double iqr = percentile(sorted, valid_count, 0.75) -
                       percentile(sorted, valid_count, 0.25);

    set(output, EcgAfFeatureIndex::MeanRr, mean_rr);
    set(output, EcgAfFeatureIndex::MedianRr, median);
    set(output, EcgAfFeatureIndex::Sdnn, sdnn);
    set(output, EcgAfFeatureIndex::Rmssd, rmssd);
    set(output, EcgAfFeatureIndex::Pnn50,
        100.0 * static_cast<double>(nn50) / static_cast<double>(difference_count));
    set(output, EcgAfFeatureIndex::CvRr, mean_rr > 0.0 ? sdnn / mean_rr : 0.0);
    set(output, EcgAfFeatureIndex::IqrRr, iqr);
    set(output, EcgAfFeatureIndex::MinRr, sorted[0]);
    set(output, EcgAfFeatureIndex::MaxRr, sorted[valid_count - 1U]);

    for (float value : output.values) {
        if (!std::isfinite(value)) {
            output.status = FeatureVectorStatus::InvalidInput;
            return output;
        }
    }
    output.status = FeatureVectorStatus::Ready;
    return output;
}

}
