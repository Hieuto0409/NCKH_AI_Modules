#include "ui/oled_ui.h"

namespace ppgfw {

bool OledUi::begin() {
    return driver_.begin();
}

void OledUi::render(MeasurementState state, const ResultSnapshot& result,
                    uint32_t remaining_ms, bool ppg_available, bool contact,
                    bool lead_off) {
    driver_.render(state, result, remaining_ms, ppg_available, contact, lead_off);
}

}
