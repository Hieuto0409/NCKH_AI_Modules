#include "drivers/esp32_adc_backend.h"

#include "config/pins.h"

#include <Arduino.h>

namespace ppgfw {

bool Esp32AdcBackend::begin() {
    pinMode(config::pins::kEcgOut, INPUT);
    pinMode(config::pins::kEcgLeadOffPositive, INPUT);
    pinMode(config::pins::kEcgLeadOffNegative, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(config::pins::kEcgOut, ADC_11db);
    return true;
}

int16_t Esp32AdcBackend::readRaw() {
    return static_cast<int16_t>(analogRead(config::pins::kEcgOut));
}

bool Esp32AdcBackend::leadOff() const {
    return digitalRead(config::pins::kEcgLeadOffPositive) == HIGH ||
           digitalRead(config::pins::kEcgLeadOffNegative) == HIGH;
}

const char* Esp32AdcBackend::name() const {
    return "ESP32_ADC1_ONESHOT_CANDIDATE";
}

}

