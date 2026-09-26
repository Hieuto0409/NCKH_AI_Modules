#include "acquisition/ppg_acquisition.h"
#include "acquisition/sample_integrity.h"
#include "acquisition/timestamp_service.h"
#include "config/sampling.h"
#include "config/thresholds.h"
#include <algorithm>
namespace ppgfw {
PpgAcquisition::PpgAcquisition(Max30102Driver& driver, PpgSampleQueue& queue) : driver_(driver), queue_(queue) {}
bool PpgAcquisition::begin() {
    available_ = driver_.begin();
    active_ = available_;
    latest_ir_ = 0;
    queue_.clear();
    pending_discontinuity_ = true;
    last_timestamp_us_ = 0;
    last_poll_us_ = TimestampService::nowUs();
    return available_;
}
bool PpgAcquisition::setActive(bool active) {
    if (active) return begin();
    active_ = false;
    latest_ir_ = 0;
    queue_.clear();
    return driver_.shutdown();
}
void PpgAcquisition::poll() {
    if (!available_ || !active_) return;
    const uint64_t started = TimestampService::nowUs();
    const bool stale = last_poll_us_ && started - last_poll_us_ > 32 * config::kPpgSamplePeriodUs;
    last_poll_us_ = started;
    PpgFifoSample batch[32]{};
    const auto result = driver_.drain(batch, 32, stale);
    if (result.discontinuity) {
        if (!pending_discontinuity_) {
            // Coalesce one outage until the first recovered sample. Unknown
            // loss is a lower bound, never a fabricated precise sample count.
            diagnostics_.dropped_samples += result.lost_samples;
            diagnostics_.expected_samples += result.lost_samples;
            ++diagnostics_.overflow_count;
            sequence_ += result.lost_samples;
        }
        pending_discontinuity_ = true;
        last_timestamp_us_ = 0;
        latest_ir_ = 0;
    }
    if (result.count) {
        const auto plan = PpgTimestampReconstructor::plan(TimestampService::nowUs(), result.count,
            last_timestamp_us_, config::kPpgSamplePeriodUs);
        diagnostics_.expected_samples += result.count;
        for (size_t i=0; i<result.count; ++i) {
            const uint64_t timestamp = plan.first_timestamp_us + i * config::kPpgSamplePeriodUs;
            uint16_t flags = SampleIntegrity::ppgFlags(batch[i], false);
            if (pending_discontinuity_) flags |= SampleDropoutContext;
            PpgSample sample{timestamp, sequence_++, batch[i].red, batch[i].ir, flags};
            ++diagnostics_.received_samples;
            latest_ir_ = sample.ir;
            if (flags & SampleClipping) ++diagnostics_.clipping_count;
            if (!queue_.push(sample)) {
                ++diagnostics_.queue_drop_count;
                pending_discontinuity_ = true;
            } else pending_discontinuity_ = false;
            last_timestamp_us_ = timestamp;
        }
    }
    diagnostics_.queue_high_water_mark = queue_.highWaterMark();
    diagnostics_.worst_poll_latency_us = std::max(diagnostics_.worst_poll_latency_us,
        static_cast<uint32_t>(TimestampService::nowUs()-started));
}
void PpgAcquisition::resetWindowDiagnostics() { diagnostics_ = {}; }
IntegrityDiagnostics PpgAcquisition::diagnostics() const { return diagnostics_; }
bool PpgAcquisition::sensorAvailable() const { return available_; }
bool PpgAcquisition::contactDetected() const { return latest_ir_ >= config::candidate::kFingerIrMinimum; }
uint32_t PpgAcquisition::latestIr() const { return latest_ir_; }
}
