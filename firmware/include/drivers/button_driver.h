#pragma once

#include <cstdint>

namespace ppgfw {

class ButtonDriver {
public:
    ButtonDriver(int8_t pin, uint32_t debounce_ms = 35);
    void begin();
    void update(uint32_t now_ms);
    bool pressed();

private:
    int8_t pin_{};
    uint32_t debounce_ms_{};
    uint32_t changed_at_ms_{};
    bool raw_state_{};
    bool stable_state_{};
    bool pressed_event_{};
};

}

