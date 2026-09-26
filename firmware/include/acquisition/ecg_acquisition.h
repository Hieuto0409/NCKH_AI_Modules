#pragma once

#include "acquisition/sample_queues.h"
#include "drivers/ecg_adc_backend.h"
#include "types/runtime_types.h"

#include <cstdint>
#include "acquisition/timestamp_service.h"
#if !defined(PPGFW_NATIVE_TEST)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#endif

namespace ppgfw {

class EcgAcquisition {
public:
    using Clock = uint64_t (*)();
    EcgAcquisition(EcgAdcBackend& backend, EcgSampleQueue& queue, Clock clock = TimestampService::nowUs);
    bool begin();
    void poll();
    bool setActive(bool active);
    void resetWindowDiagnostics();
    IntegrityDiagnostics diagnostics() const;
    bool leadOff() const;

private:
    Clock clock_;
#if !defined(PPGFW_NATIVE_TEST)
    static void timerCallback(void* context);
    static void taskEntry(void* context);
    TaskHandle_t task_{};
    esp_timer_handle_t timer_{};
    SemaphoreHandle_t mutex_{};
#endif
    EcgAdcBackend& backend_;
    EcgSampleQueue& queue_;
    IntegrityDiagnostics diagnostics_{};
    uint32_t sequence_{};
    uint64_t next_sample_us_{};
    bool available_{};
    bool active_{};
    bool lead_off_{};
    bool pending_dropout_context_{};
};

}
