#include "drivers/oled_driver.h"

#include "app/measurement_state_machine.h"
#include "config/pins.h"

#include "ui/oled_frame.h"

namespace ppgfw {

OledDriver::OledDriver()
#if APP_ENABLE_OLED
    : display_(U8G2_R0, config::pins::kOledSclk, config::pins::kOledMosi,
               config::pins::kOledCs, config::pins::kOledDc, config::pins::kOledReset)
#endif
{
}

bool OledDriver::begin() {
#if APP_ENABLE_OLED
    available_ = display_.begin();
    if (available_) {
        display_.setFont(u8g2_font_6x10_tf);
    }
#else
    available_ = false;
#endif
    return available_;
}

void OledDriver::setSleeping(bool sleeping) {
#if APP_ENABLE_OLED
    if (available_ && sleeping != sleeping_) display_.setPowerSave(sleeping ? 1 : 0);
#endif
    sleeping_ = sleeping;
}

void OledDriver::render(MeasurementState state, const ResultSnapshot& result,
                        uint32_t remaining_ms, bool ppg_available, bool contact,
                        bool lead_off, uint8_t page, UploadSnapshot upload) {
#if APP_ENABLE_OLED
    if (!available_ || sleeping_) return;
    const auto frame = makeOledFrame(state, result, ppg_available, contact, lead_off, page, upload);
    display_.clearBuffer();
    display_.setFont(u8g2_font_6x10_tf);
    constexpr uint8_t baselines[] = {10, 21, 31, 41, 51, 62};
    for (unsigned i = 0; i < frame.rows.size(); ++i)
        display_.drawStr(0, baselines[i], frame.rows[i].data());
    display_.sendBuffer();
#else
    (void)state; (void)result; (void)ppg_available; (void)contact; (void)lead_off;
    (void)page; (void)upload;
#endif
    (void)remaining_ms;
}
}
