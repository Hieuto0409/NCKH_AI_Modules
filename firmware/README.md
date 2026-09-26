# ECG PPG Edge AI firmware 0.4.0

PlatformIO firmware for ESP32-S3 DevKitC-1 N16R8 with MAX30102 and AD8232.

This directory contains the integrated device firmware. The standalone Task 2
C/Arduino signal-processing reference is in [../tin_hieu](../tin_hieu).
Raw captures and research datasets are not included in this directory.

## Build

```powershell
cd firmware
pio run -e esp32-s3-devkitc-1
pio test -e native
```

Host capture tools are documented in `tools/README.md`.
Historical application-binary SHA-256 hashes are in `dist/README.md`.
Compiled binaries are excluded from Git; build from this source before flashing.

On Windows, the Xtensa toolchain may reject Unicode project paths. From `firmware/`, use
`powershell -ExecutionPolicy Bypass -File tools/build_windows.ps1`; it stages the
project in a temporary ASCII-only directory, copies the build artifacts back, and
removes the temporary directory afterward.

## Runtime

- BTN1 starts a measurement and advances from the result screen.
- BTN2 cancels the current measurement.
- MAX30102 runs Red and IR at 200 Hz, average 1. The direct FIFO reader drains
  full RED/IR pairs; SparkFun 1.1.2 is used only for setup, not its four-slot queue.
- AD8232 uses a timer-notified acquisition task with ADC1 oneshot at target 500 Hz;
  actual read timestamps and missed slots are recorded, with no historical backfill.
- Serial monitor speed is 921600 baud.
- Raw binary, CSV, MQTT, and OLED are independently controlled by build flags.
- Optional profiles: `esp32-s3-devkitc-1-mqtt`, `esp32-s3-devkitc-1-raw-log`, and
  the explicit `esp32-s3-devkitc-1-fixture` offline test profile.

The production path uses real sensor acquisition only. Synthetic inputs exist only
in the explicit fixture profile, which prints `OFFLINE TEST — NOT A SENSOR
MEASUREMENT` before producing output.

The pinned AI integration is from `Hieuto0409/NCKH_AI_Modules` commit
`7d4683581c8c01ff6878bcad31a601aff271f2a0`:

- PPG peaks feed a dedicated 60 s, 14-feature Stress adapter and the pinned
  StandardScaler/logistic-regression header.
- ECG selects a clean 30 s window from seven candidates, 5 s apart. Only RR and
  SQI within that selected window feed the 9-feature Edge Impulse AF/non-AF contract.
  `non-AF` is never presented as general rhythm normality.
- Raw synchronized Red/IR remain at 200 Hz globally. A deterministic two-sample
  boxcar adapter supplies 100 Hz pairs to `ResearchSpO2::Stream100`.

SpO2 calibration, SQI thresholds, ECG/PPG filters, and peak detectors remain
candidate implementations until bench and reference validation. A software-accepted
SpO2 window is not clinical validation. The reported Stress accuracy of 93.33% is a
WESAD S13+S16 evaluation result, not MAX30102 accuracy.

The generated Edge Impulse files retain their original license headers. Those
headers state that use requires an eligible active paid Edge Impulse subscription;
confirm licensing before distribution or deployment. See `CODEX_INTEGRATION_REPORT.md`
and `lib/PROVENANCE.md` for the complete integration record.

## Compatibility repair 0.3.0

Warmup feeds DSP; one 64-bit microsecond clock defines the 60 s half-open session.
PPG timestamps follow the sample grid across poll jitter. Transport loss resets
continuity and rejects affected windows. SpO2 uses its latest 4 s of raw pairs,
with the conservative 60 s PPG quality gate explicitly retained. Pulse BPM uses
the cleaned median PPI used by Stress. Feature schema is 3, raw record schema 2.

See [repair report](../docs/ai-compatibility/REPAIR_REPORT.md) for test results,
real host ECG inference, quantization details and required board measurements.
Native Unity still uses an ECG model stub; `tools/host_inference/run.py` exercises
the actual export separately. Neither test establishes clinical performance.

## Battery operation and ThingsBoard (0.4.0)

See [power/noise review](../docs/power-and-noise/REVIEW.md) and
[validation](../docs/power-and-noise/VALIDATION_RESULTS.md). Wi-Fi is off during
acquisition; the MQTT profile connects only after the result, waits for QoS1 ACK,
and turns the radio off after success/error/30 s budget. Pending results use a
four-entry RAM queue. Configure the ignored `include/config/network_secrets.h`
from the example header; default firmware remains offline.

MAX30102 shuts down and ECG sampling pauses outside measurement/contact. OLED is
off during warmup/measurement and after 15 s idle; buttons still work. Idle may
enter light sleep with button GPIO wake and a one-second timer fallback. Raw-log
profiles intentionally retain continuous acquisition. Board current, RF noise,
sleep/wake, sensor resume and real ThingsBoard delivery still need bench tests.
