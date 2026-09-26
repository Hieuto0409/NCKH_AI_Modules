#pragma once

#include "types/metric_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ppgfw {

class RunningStatistics {
public:
    void reset() {
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
        minimum_ = std::numeric_limits<double>::infinity();
        maximum_ = -std::numeric_limits<double>::infinity();
    }

    void add(double value) {
        ++count_;
        const double delta = value - mean_;
        mean_ += delta / static_cast<double>(count_);
        const double delta2 = value - mean_;
        m2_ += delta * delta2;
        minimum_ = std::min(minimum_, value);
        maximum_ = std::max(maximum_, value);
    }

    SignalStatistics snapshot() const {
        SignalStatistics result{};
        result.count = count_;
        result.mean = mean_;
        result.variance = count_ > 1 ? m2_ / static_cast<double>(count_ - 1U) : 0.0;
        result.minimum = count_ > 0 ? minimum_ : 0.0;
        result.maximum = count_ > 0 ? maximum_ : 0.0;
        return result;
    }

    double standardDeviation() const {
        return std::sqrt(snapshot().variance);
    }

private:
    uint32_t count_{};
    double mean_{};
    double m2_{};
    double minimum_{std::numeric_limits<double>::infinity()};
    double maximum_{-std::numeric_limits<double>::infinity()};
};

class RunningCorrelation {
public:
    void reset() {
        count_ = 0;
        mean_x_ = 0.0;
        mean_y_ = 0.0;
        covariance_ = 0.0;
        variance_x_ = 0.0;
        variance_y_ = 0.0;
    }

    void add(double x, double y) {
        ++count_;
        const double dx = x - mean_x_;
        mean_x_ += dx / count_;
        const double dy = y - mean_y_;
        mean_y_ += dy / count_;
        covariance_ += dx * (y - mean_y_);
        variance_x_ += dx * (x - mean_x_);
        variance_y_ += dy * (y - mean_y_);
    }

    float value() const {
        const double denominator = std::sqrt(variance_x_ * variance_y_);
        return denominator > 0.0 ? static_cast<float>(covariance_ / denominator) : 0.0F;
    }

private:
    uint32_t count_{};
    double mean_x_{};
    double mean_y_{};
    double covariance_{};
    double variance_x_{};
    double variance_y_{};
};

}

