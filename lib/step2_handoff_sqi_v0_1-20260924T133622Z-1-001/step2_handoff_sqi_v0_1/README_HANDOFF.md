# Step 2 — Hardware SQI handoff v0.1

## Status

Step 2 is **frozen for the prototype/demo**. The raw capture files are kept
unchanged. The thresholds below are engineering quality gates, not medical
diagnosis thresholds.

The verified hardware timing is:

- ECG: 500 Hz (`dt` about 2,000 microseconds)
- PPG: 25 Hz (`dt` about 40,000 microseconds)
- SQI window: 5 seconds
- SQI step: 2.5 seconds

No new hardware measurement is required for the offline processing stage.
Remeasure only if the firmware, wiring, sensor configuration, FIFO mapping,
or capture conditions change.

## Source captures

The `raw/` folder contains the four raw CSV fixtures used for the baseline:

| Role | Source capture | Use |
|---|---|---|
| PPG reference | `ppg_raw(5)(1).csv` | Resting/contact PPG reference, 25 Hz |
| ECG clean long | `ecg_raw(8)(1).csv` | Long clean ECG reference, 500 Hz |
| ECG degraded | `ecg_raw(9)(1).csv` | Reject/noisy ECG reference, 500 Hz |
| ECG clean short | `ecg_raw(5)(1).csv` | Short clean ECG reference, about 10 s |

The suffix `(1)` may be added by the file-download system. It does not change
the capture identity.

## SQI files

The `sqi_calibration_v01/` folder contains:

- `calibrate_hardware_sqi.py`: reproducible calibration script.
- `ecg_sqi_windows.csv`: ECG metrics and per-window decisions.
- `ppg_sqi_windows.csv`: PPG metrics and per-window decisions.
- `sqi_thresholds_v0_1.json`: frozen v0.1 threshold configuration.

## v0.1 thresholds

### ECG

- `clean_fraction >= 0.80`
- `adc_invalid_fraction <= 0.005`
- `rail_fraction <= 0.005`
- `flat_ratio <= 0.05`

### PPG

- `slot0_perfusion >= 0.0010`
- `slot1_perfusion >= 0.0024`
- `flat_ratio <= 0.05`
- at least 3 peaks per 5-second window
- `fifo_valid_fraction >= 1.0`

## Baseline result

- ECG clean-long reference: 22/23 windows accepted.
- ECG clean-short reference: 3/3 windows accepted.
- ECG degraded reference: 0/23 windows accepted.
- PPG reference: 23/23 windows accepted.

The combined ECG acceptance rate is not a device score; it includes the
intentionally degraded reference used to verify rejection behavior.

## Known limitations

- The PPG set has no deliberate finger-off negative capture.
- `finger_contact` is unavailable in this baseline (`-1`).
- `slot0_raw` and `slot1_raw` are not renamed Red/IR until a controlled FIFO
  mapping test is completed.
- This SQI is signal-quality gating only, not a medical accuracy or diagnosis
  claim.

## Next stage: offline processing

Process ECG and PPG on separate sample-rate branches. Do not apply one common
500-Hz pipeline to both signals.

1. ECG 500 Hz: validate → detrend → band-pass 0.5–40 Hz → SQI gate → R-peak,
   IBI and BPM features.
2. PPG 25 Hz: validate → separate slot0/slot1 → DC removal/detrend →
   band-pass suitable for 25 Hz → SQI gate → peaks, perfusion and PPG features.
3. Export filtered signals, quality flags and valid feature rows for the AI
   dataset.
4. Convert the frozen JSON values into C constants and replay tests for the
   firmware implementation.

