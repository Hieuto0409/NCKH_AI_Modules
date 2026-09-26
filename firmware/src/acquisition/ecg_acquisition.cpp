#include "acquisition/ecg_acquisition.h"
#include "acquisition/sample_integrity.h"
#include "config/sampling.h"
#include <algorithm>
namespace ppgfw {
EcgAcquisition::EcgAcquisition(EcgAdcBackend& backend, EcgSampleQueue& queue, Clock clock)
    : clock_(clock), backend_(backend), queue_(queue) {}
#if !defined(PPGFW_NATIVE_TEST)
void EcgAcquisition::timerCallback(void* context) {
    auto* self = static_cast<EcgAcquisition*>(context);
    if (self->task_) xTaskNotifyGive(self->task_);
}
void EcgAcquisition::taskEntry(void* context) {
    auto* self = static_cast<EcgAcquisition*>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // coalesce late ticks, never backfill
        self->poll();
    }
}
#endif
bool EcgAcquisition::begin() {
#if !defined(PPGFW_NATIVE_TEST)
    if (timer_) return available_;
    if (!mutex_) mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) return false;
#endif
    available_ = backend_.begin();
    next_sample_us_ = clock_();
#if !defined(PPGFW_NATIVE_TEST)
    if (!available_) return false;
    if (xTaskCreatePinnedToCore(taskEntry, "ecg-adc", 4096, this, 3, &task_, 0) != pdPASS) {
        available_ = false; return false;
    }
    esp_timer_create_args_t args{};
    args.callback = timerCallback; args.arg = this; args.dispatch_method = ESP_TIMER_TASK;
    args.name = "ecg-500hz"; args.skip_unhandled_events = true;
    if (esp_timer_create(&args, &timer_) != ESP_OK ||
        esp_timer_start_periodic(timer_, config::kEcgSamplePeriodUs) != ESP_OK) {
        if (timer_) esp_timer_delete(timer_);
        timer_ = nullptr; vTaskDelete(task_); task_ = nullptr; available_ = false;
    }
#endif
    return available_;
}
void EcgAcquisition::poll() {
#if !defined(PPGFW_NATIVE_TEST)
    xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    const uint64_t started = clock_();
    if (available_ && started >= next_sample_us_) {
        const uint32_t skipped = static_cast<uint32_t>((started-next_sample_us_)/config::kEcgSamplePeriodUs);
        diagnostics_.dropped_samples += skipped;
        diagnostics_.expected_samples += skipped + 1;
        sequence_ += skipped;
        lead_off_ = backend_.leadOff();
        const uint64_t before = clock_();
        const int16_t raw = backend_.readRaw();
        const uint64_t after = clock_();
        const uint64_t timestamp = before + (after-before)/2;
        const uint16_t flags = SampleIntegrity::ecgFlags(raw, lead_off_, skipped || pending_dropout_context_);
        const EcgSample sample{timestamp, sequence_++, raw, flags};
        ++diagnostics_.received_samples;
        if (flags & SampleClipping) ++diagnostics_.clipping_count;
        if (flags & SampleLeadOff) ++diagnostics_.lead_off_count;
        pending_dropout_context_ = !queue_.push(sample);
        if (pending_dropout_context_) ++diagnostics_.queue_drop_count;
        diagnostics_.queue_high_water_mark = queue_.highWaterMark();
        diagnostics_.worst_poll_latency_us = std::max(diagnostics_.worst_poll_latency_us,
            static_cast<uint32_t>(after - next_sample_us_));
        next_sample_us_ += (static_cast<uint64_t>(skipped) + 1) * config::kEcgSamplePeriodUs;
    }
#if !defined(PPGFW_NATIVE_TEST)
    xSemaphoreGive(mutex_);
#endif
}
void EcgAcquisition::resetWindowDiagnostics() {
#if !defined(PPGFW_NATIVE_TEST)
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    diagnostics_ = {};
#if !defined(PPGFW_NATIVE_TEST)
    xSemaphoreGive(mutex_);
#endif
}
IntegrityDiagnostics EcgAcquisition::diagnostics() const {
#if !defined(PPGFW_NATIVE_TEST)
    if (!mutex_) return {};
    xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    auto result = diagnostics_;
#if !defined(PPGFW_NATIVE_TEST)
    xSemaphoreGive(mutex_);
#endif
    return result;
}
bool EcgAcquisition::leadOff() const {
#if !defined(PPGFW_NATIVE_TEST)
    if (!mutex_) return true;
    xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    const bool value = lead_off_;
#if !defined(PPGFW_NATIVE_TEST)
    xSemaphoreGive(mutex_);
#endif
    return value;
}
}
