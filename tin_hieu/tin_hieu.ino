#include <math.h>

#include "config.h"
#include "SignalProcessor.h"

/* 1 = dummy software test; 0 = hardware driver path. */
#define USE_DUMMY_INPUT 1

#if USE_DUMMY_INPUT == 0
#include <Wire.h>
#include "max30102.h"
#include "ad8232.h"
#endif

/*
 * 1 = compile/run without hardware using synthetic signals.
 * Set to 0 only after adding the real MAX30102 + AD8232 acquisition code.
 */
static SignalProcessor processor;
static SignalResult result;
static uint32_t dummy_index = 0U;
static uint32_t next_tick_us = 0U;

#if USE_DUMMY_INPUT == 0
static Max30102State max_state;
static Ad8232State ecg_state;
static bool hardware_initialized = false;
#endif

static bool read_dummy(float *ecg, float *red, float *ir)
{
    const float fs = DEFAULT_FS_HZ;
    const float t = (float)dummy_index / fs;
    const float pi2 = 6.28318530718f;

    /* Baseline + pulse-like components; only for software smoke testing. */
    *ecg = 2048.0f + 120.0f * sinf(pi2 * 1.2f * t)
                 + 25.0f * sinf(pi2 * 15.0f * t);
    *red = 100000.0f + 100.0f * sinf(pi2 * 1.2f * t);
    *ir  =  80000.0f +  50.0f * sinf(pi2 * 1.2f * t);

    ++dummy_index;
    return true;
}

static bool read_hardware(float *ecg, float *red, float *ir)
{
#if USE_DUMMY_INPUT == 0
    uint32_t red_u32 = 0U;
    uint32_t ir_u32 = 0U;
    uint16_t ecg_u16 = 0U;
    if (!hardware_initialized) {
        if (!max30102_init(&max_state) || !ad8232_init(&ecg_state)) {
            return false;
        }
        hardware_initialized = true;
    }
    if (!max30102_read_sample(&max_state, &red_u32, &ir_u32)
        || !ad8232_read_sample(&ecg_state, &ecg_u16)) {
        return false;
    }
    *ecg = (float)ecg_u16;
    *red = (float)red_u32;
    *ir = (float)ir_u32;
    return true;
#else
    (void)ecg;
    (void)red;
    (void)ir;
    return false;
#endif
}

static uint32_t get_dropped_samples(void)
{
#if USE_DUMMY_INPUT == 0
    return max_state.overflow_count;
#else
    return 0U;
#endif
}

static void halt_with_error(const char *message)
{
    Serial.print("# ERROR: ");
    Serial.println(message);
    Serial.println("# Fix config.h/driver pins, then reset.");
    while (1) delay(1000);
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(300);

#if USE_DUMMY_INPUT == 0
    if (PIN_I2C_SDA == 255U || PIN_I2C_SCL == 255U
        || PIN_ECG_OUTPUT == 255U || PIN_LO_PLUS == 255U
        || PIN_LO_MINUS == 255U) {
        halt_with_error("Hardware pins are still 255 placeholders");
    }
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    analogReadResolution(12);
#endif

    /* Temporary fs for dummy mode. Replace with measured fs on real HW. */
    sp_init(&processor, DEFAULT_FS_HZ);

    if (processor.config_clamped) {
        halt_with_error("window_samples clamped; increase MAX_WINDOW_SAMPLES");
    }

    Serial.println("# SignalProcessor reference v0.4");
#if SERIAL_STREAM_MODE == SERIAL_STREAM_RAW_ONLY
    Serial.println("# CSV: S,timestamp_us,sample_index,ecg_raw,red_raw,ir_raw,window_ready,dropped_samples");
#else
    Serial.println("# CSV: SAMPLE,timestamp_us,sample_index,ecg_raw,red_raw,ir_raw,ecg_filtered,ecg_qrs,red_ac,ir_ac,red_dc,ir_dc,window_ready,ecg_good,ppg_good,both_good,quality_reason_mask,window_gap_count,gap_count,dropped_samples");
#endif
#if USE_DUMMY_INPUT
    Serial.println("# WARNING: dummy mode; no hardware accuracy claim");
#else
    Serial.println("# Mode: hardware input; validation pending");
#endif
}

void loop()
{
    uint32_t now_us;
    float ecg_raw;
    float red_raw;
    float ir_raw;
    bool ok;

    if (USE_DUMMY_INPUT) {
        now_us = micros();
        if ((int32_t)(now_us - next_tick_us) < 0) return;
        next_tick_us = now_us + (uint32_t)(1000000.0f / DEFAULT_FS_HZ);
        ok = read_dummy(&ecg_raw, &red_raw, &ir_raw);
    } else {
        now_us = micros();
        ok = read_hardware(&ecg_raw, &red_raw, &ir_raw);
    }

    if (!ok) return;

    if (sp_process_sample(&processor, ecg_raw, red_raw, ir_raw,
                          now_us, &result) != 0) {
        Serial.println("ERROR,signal_processor");
        return;
    }

#if SERIAL_STREAM_MODE == SERIAL_STREAM_RAW_ONLY
    /* Lightweight row: keep raw data and sample index for replay. */
    Serial.print("S,");
    Serial.print(now_us);
    Serial.print(",");
    Serial.print(processor.sample_index);
    Serial.print(",");
    Serial.print(ecg_raw, 1);
    Serial.print(",");
    Serial.print(red_raw, 1);
    Serial.print(",");
    Serial.print(ir_raw, 1);
    Serial.print(",");
    Serial.print(result.window_ready);
    Serial.print(",");
    Serial.println(get_dropped_samples());
#else
    /* Full row for bench/debug; use only when UART bandwidth is sufficient. */
    Serial.print("SAMPLE,");
    Serial.print(now_us);
    Serial.print(",");
    Serial.print(processor.sample_index);
    Serial.print(",");
    Serial.print(ecg_raw, 3);
    Serial.print(",");
    Serial.print(red_raw, 3);
    Serial.print(",");
    Serial.print(ir_raw, 3);
    Serial.print(",");
    Serial.print(result.ecg_filtered, 6);
    Serial.print(",");
    Serial.print(result.ecg_qrs, 6);
    Serial.print(",");
    Serial.print(result.red_ac, 6);
    Serial.print(",");
    Serial.print(result.ir_ac, 6);
    Serial.print(",");
    Serial.print(result.red_dc, 6);
    Serial.print(",");
    Serial.print(result.ir_dc, 6);
    Serial.print(",");
    Serial.print(result.window_ready);
    Serial.print(",");
    Serial.print(result.ecg_good);
    Serial.print(",");
    Serial.print(result.ppg_good);
    Serial.print(",");
    Serial.print(result.both_good);
    Serial.print(",");
    Serial.print(result.quality_reason_mask);
    Serial.print(",");
    Serial.print(result.window_gap_count);
    Serial.print(",");
    Serial.print(result.gap_count);
    Serial.print(",");
    Serial.println(get_dropped_samples());
#endif

    if (result.window_ready) {
        Serial.print("WINDOW,");
        Serial.print(result.window_index);
        Serial.print(",red_perf=");
        Serial.print(result.red_perfusion, 6);
        Serial.print(",ir_perf=");
        Serial.print(result.ir_perfusion, 6);
        Serial.print(",ecg_good=");
        Serial.print(result.ecg_good);
        Serial.print(",ppg_good=");
        Serial.print(result.ppg_good);
        Serial.print(",both_good=");
        Serial.print(result.both_good);
        Serial.print(",reason_mask=");
        Serial.print(result.quality_reason_mask);
        Serial.print(",window_gaps=");
        Serial.println(result.window_gap_count);
    }
}
