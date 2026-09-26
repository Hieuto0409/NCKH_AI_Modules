# ECG/PPG Signal Processing

Task 2 signal-processing reference for ECG and PPG: filters, peak detection,
interval features, signal-quality checks, sensor-driver skeletons, and host tools.
This repository contains the `tin_hieu_reference_v0.4` source package.

## Repository boundary

- This directory contains the standalone C/Arduino reference and its tests.
- Integrated ESP32-S3 firmware, OLED, MQTT, and AI adapters live in
  [../firmware](../firmware).
- Raw captures and research datasets are not included.

## Getting started

Clone the project repository and open `tin_hieu/tin_hieu.ino` in the Arduino IDE.
The sketch directory already has the matching name:

```sh
git clone https://github.com/Hieuto0409/EdgeAI-PPG-Screening.git
cd EdgeAI-PPG-Screening/tin_hieu
```

Read [README_PORT.md](README_PORT.md) for build, capture, and porting instructions.
Run the two host checks documented in [tests/README.md](tests/README.md).
Python capture and analysis dependencies are in
[tools/requirements.txt](tools/requirements.txt).

## Reference documents

- [MANIFEST.md](MANIFEST.md): source inventory and checksums.
- [FORMULAS.md](FORMULAS.md): feature definitions and quality conventions.
- [HARDWARE_INTEGRATION.md](HARDWARE_INTEGRATION.md): hardware integration.
- [HARDWARE_TUNING.md](HARDWARE_TUNING.md): calibration points.
- [PRE_HARDWARE_STATUS.md](PRE_HARDWARE_STATUS.md): implementation status.

The drivers and thresholds remain engineering references requiring hardware
validation. Software tests do not establish physiological or clinical accuracy.
