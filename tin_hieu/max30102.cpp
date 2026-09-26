#include "max30102.h"

#include "config.h"

#include <Arduino.h>
#include <Wire.h>

/* This is a driver skeleton, not a final sensor configuration. Verify LED
 * current, pulse width and sample rate on the actual module. */
#define MAX30102_SPO2_CONFIG_DEFAULT 0x27U /* provisional: 100 Hz, 411 us */
#define MAX30102_FIFO_CONFIG_DEFAULT 0x4FU /* avg=4, rollover enabled */
#define MAX30102_RED_CURRENT_DEFAULT 0x24U
#define MAX30102_IR_CURRENT_DEFAULT  0x24U

static bool i2c_write_reg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission((uint8_t)MAX30102_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool i2c_read_reg(uint8_t reg, uint8_t *value)
{
    if (value == NULL) return false;
    Wire.beginTransmission((uint8_t)MAX30102_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)MAX30102_ADDR, (uint8_t)1) != 1U) {
        return false;
    }
    *value = Wire.read();
    return true;
}

static bool i2c_read_bytes(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    if (buffer == NULL) return false;
    Wire.beginTransmission((uint8_t)MAX30102_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)MAX30102_ADDR, length) != length) {
        return false;
    }
    for (i = 0U; i < length; ++i) buffer[i] = Wire.read();
    return true;
}

bool max30102_init(Max30102State *state)
{
    uint8_t part_id;
    if (state == NULL) return false;
    state->initialized = false;
    state->sample_count = 0U;
    state->overflow_count = 0U;
    state->part_id = 0U;

    if (!i2c_read_reg(MAX30102_REG_PART_ID, &part_id)) return false;
    state->part_id = part_id;
    if (part_id != MAX30102_PART_ID_VALUE) return false;

    if (!i2c_write_reg(MAX30102_REG_MODE_CONFIG, 0x40U)) return false;
    delay(100U);
    if (!i2c_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00U)
        || !i2c_write_reg(MAX30102_REG_OVF_COUNTER, 0x00U)
        || !i2c_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00U)
        || !i2c_write_reg(MAX30102_REG_FIFO_CONFIG,
                          MAX30102_FIFO_CONFIG_DEFAULT)
        || !i2c_write_reg(MAX30102_REG_SPO2_CONFIG,
                          MAX30102_SPO2_CONFIG_DEFAULT)
        || !i2c_write_reg(MAX30102_REG_LED1_PA,
                          MAX30102_RED_CURRENT_DEFAULT)
        || !i2c_write_reg(MAX30102_REG_LED2_PA,
                          MAX30102_IR_CURRENT_DEFAULT)
        || !i2c_write_reg(MAX30102_REG_MODE_CONFIG, 0x03U)) {
        return false;
    }
    state->initialized = true;
    return true;
}

uint8_t max30102_fifo_count(const Max30102State *state)
{
    uint8_t write_pointer = 0U;
    uint8_t read_pointer = 0U;
    (void)state;
    if (!i2c_read_reg(MAX30102_REG_FIFO_WR_PTR, &write_pointer)
        || !i2c_read_reg(MAX30102_REG_FIFO_RD_PTR, &read_pointer)) {
        return 0U;
    }
    return (uint8_t)((write_pointer - read_pointer + 32U) % 32U);
}

bool max30102_read_sample(Max30102State *state, uint32_t *red, uint32_t *ir)
{
    uint8_t overflow = 0U;
    uint8_t buffer[6];
    if (state == NULL || !state->initialized || red == NULL || ir == NULL) {
        return false;
    }
    if (max30102_fifo_count(state) == 0U) return false;
    if (i2c_read_reg(MAX30102_REG_OVF_COUNTER, &overflow)) {
        state->overflow_count += overflow;
    }
    if (!i2c_read_bytes(MAX30102_REG_FIFO_DATA, buffer, 6U)) return false;

    /* SpO2 mode FIFO order is LED1 (RED), then LED2 (IR), 18-bit MSB-first. */
    *red = ((((uint32_t)buffer[0] << 16) |
             ((uint32_t)buffer[1] << 8) | (uint32_t)buffer[2]) & 0x3FFFFU);
    *ir = ((((uint32_t)buffer[3] << 16) |
            ((uint32_t)buffer[4] << 8) | (uint32_t)buffer[5]) & 0x3FFFFU);
    state->sample_count++;
    return true;
}
