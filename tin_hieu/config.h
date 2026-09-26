#ifndef CONFIG_H
#define CONFIG_H

/*
 * Offline/reference configuration for the signal-processing module.
 *
 * DEFAULT_FS_HZ is only a compile-test value.  When the real MAX30102 and
 * AD8232 are connected, measure the actual sample rate from timestamp_us and
 * pass it to sp_init().  Do not use filter coefficients from a guessed fs.
 */
#define DEFAULT_FS_HZ 100.0f

/* The values used by the Python reference pipeline. */
#define WINDOW_SECONDS 5.0f
#define STEP_SECONDS   2.5f

/* Cut-offs are calculated at runtime from fs_hz in sp_init(). */
#define ECG_HPF_HZ 0.5f
#define ECG_LPF_HZ 35.0f
#define ECG_QRS_LO_HZ 5.0f
#define ECG_QRS_HI_HZ 20.0f

#define PPG_HPF_HZ 0.5f
#define PPG_LPF_HZ 8.0f
#define PPG_DC_LPF_HZ 0.5f

/* Frozen offline reference thresholds; recalibrate after hardware capture. */
#define PERFUSION_MIN_RED 0.0005f
#define PERFUSION_MIN_IR  0.0002f

/* Absolute ECG floors are placeholders until real ADC units are measured. */
#define ECG_RMS_MORPH_MIN 1.0f
#define ECG_RMS_QRS_MIN   0.1f

/* Causal peak defaults. These are engineering starting points only; tune
 * after observing real filtered ADC units. */
#define ECG_PEAK_LOOKAHEAD_SECONDS 0.05f
#define ECG_PEAK_REFRACTORY_SECONDS 0.30f
#define ECG_PEAK_MIN_PROMINENCE 0.01f
#define PPG_PEAK_LOOKAHEAD_SECONDS 0.05f
#define PPG_PEAK_REFRACTORY_SECONDS 0.30f
#define PPG_PEAK_MIN_PROMINENCE 0.01f
#define FEATURE_MIN_VALID_INTERVALS 2U

/* 5 s at 500 Hz = 2500 samples.  This also allows up to 600 Hz. */
#define MAX_WINDOW_SAMPLES 3000U

/* Used by the reference sketch.  Increase if the serial link drops samples. */
#define SERIAL_BAUD 921600UL

/* Serial streaming modes. RAW_ONLY is the safe default for high fs because
 * it keeps the raw samples and essential window flag while limiting UART
 * bandwidth. FULL is useful for bench/debug at lower fs. */
#define SERIAL_STREAM_RAW_ONLY 1
#define SERIAL_STREAM_FULL     2
#define SERIAL_STREAM_MODE     SERIAL_STREAM_RAW_ONLY

/* Board-specific placeholders. Set these only after checking the actual
 * ESP32-S3 module schematic. 255 means "not configured" and the hardware
 * path must fail rather than silently touching an unknown pin. */
#define PIN_I2C_SDA   255U
#define PIN_I2C_SCL   255U
#define I2C_FREQ_HZ   400000UL
#define PIN_ECG_OUTPUT 255U
#define PIN_LO_PLUS    255U
#define PIN_LO_MINUS   255U
#define LEADS_OFF_THRESHOLD 2000U

#endif /* CONFIG_H */
