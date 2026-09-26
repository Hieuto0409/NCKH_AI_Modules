#include "drivers/button_driver.h"

#include <Arduino.h>

namespace ppgfw {

ButtonDriver::ButtonDriver(int8_t pin, uint32_t debounce_ms)
    : pin_(pin), debounce_ms_(debounce_ms) {
}

void ButtonDriver::begin() {
    pinMode(pin_, INPUT_PULLUP);
    raw_state_ = digitalRead(pin_) == LOW;
    stable_state_ = raw_state_;
}

void ButtonDriver::update(uint32_t now_ms) {
    const bool current = digitalRead(pin_) == LOW;
    if (current != raw_state_) {
        raw_state_ = current;
        changed_at_ms_ = now_ms;
    }
    if (current != stable_state_ && now_ms - changed_at_ms_ >= debounce_ms_) {
        stable_state_ = current;
        if (stable_state_) {
            pressed_event_ = true;
        }
    }
}

bool ButtonDriver::pressed() {
    const bool value = pressed_event_;
    pressed_event_ = false;
    return value;
}

}

