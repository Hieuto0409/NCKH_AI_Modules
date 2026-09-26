#include "acquisition/ecg_acquisition.h"
#include "acquisition/timestamp_service.h"
#include <cstdio>

static uint64_t fake_now_us = 0;
namespace ppgfw {
uint64_t TimestampService::nowUs() { return fake_now_us; }
}
class ProbeAdc final : public ppgfw::EcgAdcBackend {
public:
    unsigned reads = 0;
    bool begin() override { return true; }
    int16_t readRaw() override { ++reads; return 2048; }
    bool leadOff() const override { return false; }
    const char* name() const override { return "PROBE_ONESHOT"; }
};
int main() {
    ProbeAdc adc;
    ppgfw::EcgSampleQueue queue;
    ppgfw::EcgAcquisition acquisition(adc, queue);
    acquisition.begin();
    fake_now_us = 10000;
    acquisition.poll();
    auto d = acquisition.diagnostics();
    std::printf("one_poll_actual_time_us=10000 adc_reads=%u received=%u dropped=%u assigned_timestamps=",
        adc.reads, d.received_samples, d.dropped_samples);
    ppgfw::EcgSample sample;
    while (queue.pop(sample)) std::printf("%llu,", static_cast<unsigned long long>(sample.timestamp_us));
    std::printf("\n");
}
