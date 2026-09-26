#pragma once

#include "config/board_config.h"
#include "types/result_types.h"

#include <cstdint>

#if APP_ENABLE_OLED
#include <U8g2lib.h>
#endif

namespace ppgfw {

enum class MeasurementState : uint8_t;

class OledDriver {
public:
    OledDriver();
    bool begin();
    void render(MeasurementState state, const ResultSnapshot& result, uint32_t remaining_ms,
                bool ppg_available, bool contact, bool lead_off);

private:
#if APP_ENABLE_OLED
    U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI display_;
#endif
    bool available_{};
};

}
