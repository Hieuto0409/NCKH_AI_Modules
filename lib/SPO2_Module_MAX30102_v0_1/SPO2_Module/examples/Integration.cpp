// Integration hooks: your existing project remains responsible for the sensor,
// FIFO, sample continuity and its signal quality assessment.
#include "ResearchSpO2.h"
static research_spo2::Stream100 spo2;
void startNewMeasurement() { spo2.reset(); }
void onSensorGapOrConfigurationChange() { spo2.reset(); }
void onRawSample100Hz(uint32_t ir, uint32_t red) {
    spo2.push(ir, red); // Not normalized, not bandpass-filtered; preserve DC.
}
research_spo2::Result finishMeasurement(bool poorSignal) {
    return spo2.evaluate(!poorSignal);
}
// In the presentation/UI layer:
// auto r = finishMeasurement(poorSignal);
// if (r.valid()) showEstimate(r.percent);
// else showNoResult(research_spo2::statusText(r.status));
// Do not display a prior percent when the current result is invalid.
