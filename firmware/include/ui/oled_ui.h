#pragma once

#include "drivers/oled_driver.h"

namespace ppgfw {

class OledUi {
public:
    bool begin();
    void setSleeping(bool sleeping) { driver_.setSleeping(sleeping); }
    void render(MeasurementState state, const ResultSnapshot& result, uint32_t remaining_ms,
                bool ppg_available, bool contact, bool lead_off);

private:
    OledDriver driver_{};
};

}
