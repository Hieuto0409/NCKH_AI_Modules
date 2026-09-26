#pragma once

#include "types/raw_samples.h"

#include <cstdint>

namespace ppgfw {

class CsvLogger {
public:
    void begin();
    void log(const PpgSample& sample);
    void log(const EcgSample& sample);
    uint32_t dropped() const;

private:
    uint32_t dropped_{};
};

}
