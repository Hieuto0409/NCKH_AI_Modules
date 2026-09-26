# Firmware artifacts

This file preserves the historical build record. The binaries listed below are
not committed to this repository. Build the matching environment from
`firmware/` before flashing.

Built for the custom `esp32-s3-devkitc-1-n16r8` board manifest.

## Current 0.2.0 integration artifacts (2026-09-26)

| File | Profile | SHA-256 |
|---|---|---|
| `firmware-default.bin` | Real sensors, OLED, DSP/SQI, pinned AI adapters | `0687E538308172A602DE03188067AB425150658356B3EE4BD500A4E10E140590` |
| `firmware-offline-fixture.bin` | Explicit synthetic fixture; not a sensor measurement | `E7739EE0C4A52D058ED18C5445249A79772E7D2EA2017EDAB959CD9B473ACD76` |

The fixture prints `OFFLINE TEST — NOT A SENSOR MEASUREMENT`. Do not use it for
measurement or data collection.

## Retained prior-baseline artifacts (not rebuilt for 0.2.0)

| File | Profile | SHA-256 |
|---|---|---|
| `firmware-mqtt.bin` | Default + Wi-Fi/MQTT adapter | `224F133425E3BD4821B7B60F5C2882F790472E635D8B3097FA23D8751B9C6F6C` |
| `firmware-raw-log.bin` | Headless binary raw capture | `B1DCCE74F8C21EB0D75A215CA06A9291C783FE0936FE709DFA391704E9B1EBBC` |

These are application images produced by PlatformIO. Prefer the matching PlatformIO
environment with `pio run -e <environment> -t upload` so the bootloader and partition
image are written at the correct offsets. Rebuild the MQTT/raw-log profiles before
using them with the 0.2.0 source tree.
