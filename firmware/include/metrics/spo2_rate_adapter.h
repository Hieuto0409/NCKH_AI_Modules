#pragma once

#include "types/metric_types.h"
#include "types/raw_samples.h"

#include <ResearchSpO2.h>
#include <cstdint>

namespace ppgfw {

struct Spo2InputConfig {
    uint32_t sample_rate_hz{};
    uint8_t red_led_current{};
    uint8_t ir_led_current{};
    uint16_t adc_range_na{};
};

class Spo2RateAdapter200To100 {
public:
    void reset(const Spo2InputConfig& config);
    void updateConfig(const Spo2InputConfig& config);
    void push(const PpgSample& sample);
    Spo2Result evaluate(bool external_quality_ok);
    uint32_t sourceSampleCount() const;
    uint32_t modulePairCount() const;
    uint32_t resetCount() const;

private:
    void resetStream(bool count_reset);
    bool sameConfig(const Spo2InputConfig& config) const;

    research_spo2::Stream100 stream_{};
    Spo2InputConfig config_{};
    PpgSample pending_{};
    uint64_t last_timestamp_us_{};
    uint32_t last_sequence_{};
    uint32_t source_sample_count_{};
    uint32_t module_pair_count_{};
    uint32_t reset_count_{};
    bool config_initialized_{};
    bool input_rejected_{};
    bool have_pending_{};
    bool have_previous_{};
};

}
