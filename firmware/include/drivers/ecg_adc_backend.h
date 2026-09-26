#pragma once

#include <cstdint>

namespace ppgfw {

class EcgAdcBackend {
public:
    virtual ~EcgAdcBackend();
    virtual bool begin() = 0;
    virtual int16_t readRaw() = 0;
    virtual bool leadOff() const = 0;
    virtual const char* name() const = 0;
};

}
