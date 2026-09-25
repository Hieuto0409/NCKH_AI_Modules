# Offline hardware processing v0.1

This folder is the first offline processing result from the frozen Step 2
hardware captures. The raw CSV files were read only and were not modified.

## Inputs

- ECG clean long: `ecg_raw(8)(1).csv`
- ECG degraded: `ecg_raw(9)(1).csv`
- ECG clean short: `ecg_raw(5)(1).csv`
- PPG reference: `ppg_raw(5)(1).csv`

## Processing axes

- ECG: 500 Hz; morphology band-pass 0.5–40 Hz; QRS branch 5–20 Hz.
- PPG: 25 Hz; slot0 and slot1 are processed independently; AC band-pass
  0.5–8 Hz and a low-pass DC estimate are exported.
- SQI windows: 5 seconds with a 2.5-second step.

## Outputs

- `*_ecg_processed.csv`: ECG raw values, filtered branches and R-peak flags.
- `ppg_processed.csv`: raw slots, DC/AC branches and pulse-peak flags.
- `*_ecg_windows.csv`: hardware SQI plus ECG peak/IBI summary per window.
- `ppg_windows.csv`: hardware SQI plus PPG peak/interval summary per window.
- `offline_summary.json`: machine-readable validation and feature summary.
- `preview_first_10s.png`: visual sanity check of the clean-long ECG and PPG.

## Result and limits

The transport and sampling checks pass on all four inputs. The frozen SQI
rules accept the clean references and reject the degraded ECG reference.
Peak-derived BPM/IBI values are an offline baseline and still require review
before being used as an AI label or a clinical claim. PPG contact remains
provisional because no deliberate finger-off capture was included.

The next handoff is:

1. AI consumes filtered signals, SQI decisions and valid feature rows.
2. Firmware ports the frozen SQI constants and replays the same window rules.

