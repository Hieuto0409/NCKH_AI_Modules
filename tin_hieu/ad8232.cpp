#include "ad8232.h"

#include "config.h"

#include <Arduino.h>

bool ad8232_init(Ad8232State *state)
{
    if (state == NULL
        || PIN_ECG_OUTPUT == 255U
        || PIN_LO_PLUS == 255U
        || PIN_LO_MINUS == 255U) {
        return false;
    }
    state->initialized = false;
    state->sample_count = 0U;
    state->lo_plus = 0U;
    state->lo_minus = 0U;
    state->leads_off = false;
    pinMode(PIN_ECG_OUTPUT, INPUT);
    pinMode(PIN_LO_PLUS, INPUT);
    pinMode(PIN_LO_MINUS, INPUT);
    state->initialized = true;
    return true;
}

bool ad8232_read_sample(Ad8232State *state, uint16_t *ecg_raw)
{
    if (state == NULL || !state->initialized || ecg_raw == NULL) {
        return false;
    }
    state->lo_plus = (uint16_t)analogRead(PIN_LO_PLUS);
    state->lo_minus = (uint16_t)analogRead(PIN_LO_MINUS);
    state->leads_off = (state->lo_plus > LEADS_OFF_THRESHOLD)
                    || (state->lo_minus > LEADS_OFF_THRESHOLD);
    *ecg_raw = (uint16_t)analogRead(PIN_ECG_OUTPUT);
    state->sample_count++;
    return true;
}
