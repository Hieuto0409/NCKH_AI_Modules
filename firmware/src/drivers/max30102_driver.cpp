#include "drivers/max30102_driver.h"
#include "config/pins.h"
#include "config/sampling.h"
namespace ppgfw {
bool Max30102Driver::begin(TwoWire& wire) {
    next_retry_ms_ = millis() + 1000;
    wire_ = &wire;
    wire_->begin(config::pins::kPpgSda, config::pins::kPpgScl, config::kI2cClockHz);
    available_ = sensor_.begin(*wire_, I2C_SPEED_FAST);
    if (!available_) return false;
    sensor_.setup(config::kMax30102RedLedCurrent, config::kMax30102SampleAverage, 2,
                  config::kPpgSampleRateHz, config::kMax30102PulseWidthUs, config::kMax30102AdcRangeNa);
    sensor_.setPulseAmplitudeRed(config::kMax30102RedLedCurrent);
    sensor_.setPulseAmplitudeIR(config::kMax30102IrLedCurrent);
    available_ = write(0x08, 0x00) && write(0x02, 0x80);
    sensor_.clearFIFO();
    uint8_t status{};
    available_ = available_ && read(0x00, &status, 1);
    return available_;
}
FifoDrainResult Max30102Driver::drain(PpgFifoSample* output, size_t capacity, bool unknown) {
    if (!available_) {
        if (wire_ && static_cast<int32_t>(millis()-next_retry_ms_) >= 0) begin(*wire_);
        return {0, 1, true, true};
    }
    uint8_t mode{}, fifo_config{}, adc_config{};
    // Locked 200 Hz, 8192 nA and 411 us. Never accept reset/default-rate data.
    static_assert(config::kPpgSampleRateHz == 200 && config::kMax30102AdcRangeNa == 8192 &&
                  config::kMax30102PulseWidthUs == 411 && config::kMax30102SampleAverage == 1);
    if (!read(0x09, &mode, 1) || !read(0x08, &fifo_config, 1) || !read(0x0a, &adc_config, 1) ||
        (mode & 0xc7) != 0x03 || fifo_config != 0x00 || (adc_config & 0x7f) != 0x4b) {
        begin(*wire_);
        return {0, 1, true, true};
    }
    return fifo_.drain(*this, output, capacity, unknown);
}
bool Max30102Driver::available() const { return available_; }
bool Max30102Driver::write(uint8_t reg, uint8_t value) {
    wire_->beginTransmission(0x57); wire_->write(reg); wire_->write(value);
    return wire_->endTransmission() == 0;
}
bool Max30102Driver::read(uint8_t reg, uint8_t* bytes, size_t count) {
    wire_->beginTransmission(0x57); wire_->write(reg);
    if (wire_->endTransmission(false) != 0) return false;
    const size_t received = wire_->requestFrom(uint8_t(0x57), uint8_t(count));
    if (received != count) { while (wire_->available()) wire_->read(); return false; }
    for (size_t i = 0; i < count; ++i) {
        if (!wire_->available()) return false;
        bytes[i] = static_cast<uint8_t>(wire_->read());
    }
    return true;
}
}
