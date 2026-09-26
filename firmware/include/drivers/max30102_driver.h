#pragma once
#include "drivers/max30102_fifo.h"
#include <MAX30105.h>
#include <Wire.h>
namespace ppgfw {
class Max30102Driver : private Max30102Bus {
public:
    bool begin(TwoWire& wire = Wire);
    FifoDrainResult drain(PpgFifoSample* output, size_t capacity, bool unknown = false);
    bool available() const;
    bool shutdown();
private:
    bool read(uint8_t reg, uint8_t* bytes, size_t count) override;
    bool write(uint8_t reg, uint8_t value) override;
    MAX30105 sensor_{};
    Max30102Fifo fifo_{};
    TwoWire* wire_{};
    bool available_{};
    uint32_t next_retry_ms_{};
};
}
