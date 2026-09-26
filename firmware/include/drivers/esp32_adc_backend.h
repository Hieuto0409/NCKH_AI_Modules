#pragma once

#include "drivers/ecg_adc_backend.h"

namespace ppgfw {

class Esp32AdcBackend final : public EcgAdcBackend {
public:
    bool begin() override;
    int16_t readRaw() override;
    bool leadOff() const override;
    const char* name() const override;
};

}

