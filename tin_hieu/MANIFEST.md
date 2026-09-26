# MANIFEST — tin_hieu_reference_v0.4

## Arduino/ESP32 source

| File | Purpose |
|---|---|
| `tin_hieu.ino` | Dummy test sketch, serial logging, hardware hook |
| `config.h` | Reference parameters and provisional hardware limits |
| `biquad.h/.c` | Runtime RBJ biquad filter design and sample step |
| `SignalProcessor.h/.c` | Six filter paths, sliding window, perfusion, SQI, timing diagnostics |
| `PeakDetector.h/.c` | Causal delayed local-peak detector with polarity/prominence/refractory |
| `FeatureExtractor.h/.c` | Guarded RR/PPI, HR/PR, SDRR/SDPPI and adjacent-valid RMSSD |
| `max30102.h/.cpp` | MAX30102 FIFO/part-ID Arduino driver skeleton; hardware values remain provisional |
| `ad8232.h/.cpp` | AD8232 ADC/leads-off Arduino driver skeleton; pins remain placeholders |
| `README_PORT.md` | Compile, test, hardware capture and handoff instructions |
| `FORMULAS.md` | Fixed formulas and quality conventions |
| `HARDWARE_TUNING.md` | Hardware-only calibration points and acquisition contract |
| `HARDWARE_INTEGRATION.md` | Safe wiring, FIFO/ADC and validation checklist |
| `PRE_HARDWARE_STATUS.md` | Frozen logic versus hardware-only decisions |

## Host tools

| File | Purpose |
|---|---|
| `tools/capture_serial.py` | Capture `S,...` or `SAMPLE,...` rows from a COM port to raw CSV |
| `tools/analyze_capture.py` | Estimate sampling rate, gaps and raw ranges |
| `tools/compare_c_vs_python.py` | Diagnostic report for C raw capture vs Python CSV |
| `tools/requirements.txt` | Python dependencies for capture/analysis |

## Tests

| File | Purpose |
|---|---|
| `tests/test_signal_processor.c` | Host smoke test for filters, overlapping windows and gap reason |
| `tests/test_peak_features.c` | Deterministic peak/feature test |
| `tests/README.md` | GCC test command and expected result |

## Scope boundary

This package is a Task 2 signal-processing reference. It includes MAX30102 and
AD8232 driver skeletons, but not final hardware validation, display, Wi-Fi,
battery management or AI model. The firmware teammate integrates those around
the pure-C modules. The peak/feature and driver modules are engineering
references, not clinical validation.

`DEFAULT_FS_HZ`, ECG RMS floors and perfusion thresholds are not final hardware
calibration. Preserve the raw CSV and timestamp diagnostics from every hardware
test.

## SHA-256

Values are generated for the exact files included in this package after the
source is finalized.

```text
51138d7b1e285bae9a96d51c2d3954317b14b786149fbc1813f834d5d04c81db  config.h
e09557d639596a8490f63182aa27c651acbdf4304303926a831b5adb8785e205  biquad.h
e8527f5c72eb944ded3e9d5306e9a2f0576b9327ce9ce82145c4283042a07e35  biquad.c
f69b4f39bb0f6ead06a0e4e4ef10fd135ddb03355c6c6287db5ded88e35579e5  SignalProcessor.h
4a33b835d4d2547e0eed53ed898dfaec0ed97199f3c0337fee71c543aa7975c3  SignalProcessor.c
ea7708ac1d340322285d4d790b3c920458e1ef87cbf6964a5523a372b9a47379  PeakDetector.h
80afbf52fdf02c679e515cbad60a2a28439c0fef6a70f4ab4c8aabd227092aea  PeakDetector.c
4393514ff5ca780ac39c5e56cb1b2aff5e1b213be461f7750c788bb3a9a796ad  FeatureExtractor.h
ea9c76046e63455c0c06bb012247b23e3f71cfe8949b7e68fb5c5345c51bd6a6  FeatureExtractor.c
f01675a84d0044a2e580f1b9947bf068ec933e2643791fe2778b47bb201e4ea0  max30102.h
9933caece0d4b5e7e50a873c6e667b037d9d7ed1917a3a949578865927b8f7c6  max30102.cpp
5d501f864cf8104156ba0e75cfd92631706c6d56497f6567508cf60760013912  ad8232.h
2a7667a45b52bd6fdee0d364561b0c2a0b4db8d6815c490dc6a5788c8ec29260  ad8232.cpp
b097089d6317295fb898896032b1b87b5ce223149af30d4d3092dff9a87fba12  tin_hieu.ino
bf4bca40b17c01f7e7659476c7dc095c10622e3425c4e66a3bef6780aaf4c286  README_PORT.md
f3fa9945ce6c0ee6c10eea4409089dfcdda9d82a2f2bed899f2d2053dbc7821b  FORMULAS.md
df74449321c95115faf18d467d8dcd4ccb2332a7890368be733538eac0edca18  HARDWARE_TUNING.md
a5e60eca01a47ec19cb89f63bb900987e386ad21e0b08f12709040009b114049  HARDWARE_INTEGRATION.md
5a1d5e56ce60758ad1dabdb9b705be7a6a4c23bdf43a671efc408ffbc905729f  PRE_HARDWARE_STATUS.md
d9a706f847b241200715bb50ff19c1847b98b7d6ccbf65cab9afe7b5ebe7db46  tools/capture_serial.py
0d0465419b3c78624637489cd8a570c70f58ff81ffff65e560d40dd400008f23  tools/analyze_capture.py
f41b0c4796399147c60696f38dee8311f926708911b5aae09c3d6d6fb9ab8825  tools/compare_c_vs_python.py
33b6ae4555a8a2be5975228d8ecb58ba2e5dd0e7d9d1a6c7e8ca60de23b74aad  tools/requirements.txt
2b095373b3e7b84998c72baea5643b4cfd62cd6542665054e586b0bb2d878014  tests/test_signal_processor.c
bf40686a8a55033e0a2b2407057cdd683410a2069c25be85cc1cf67a2fbae67c  tests/test_peak_features.c
e0ba299c3c0692ab30432a13edcdd43ce10e3695e8a434e2916acda8cc58937e  tests/README.md
9f7b58c7ea84adb1c0cc42640f9cb77958d68d7aa5ceafc45cfe7d8bc29408d0  .gitignore
```
