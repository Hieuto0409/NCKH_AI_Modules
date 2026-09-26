# Host tools

Convert a binary capture and inspect acquisition integrity:

```powershell
python tools/log_convert/log_convert.py capture.bin capture.csv
python tools/replay/replay.py capture.csv
```

The converter validates frame synchronization and CRC-16/CCITT. Record types `16-18`
carry session/device/configuration metadata; type `19` carries the pinned AI commit;
types `32-35` carry quality, metric, SpO2 and decision summaries; type `36` carries
per-branch model/scaler versions; type `37` carries branch inference latency. Replay
reports sample counts, sequence gaps, timestamp ordering, mean periods and clipping
counts. DSP and decision behavior is covered by the native C++ tests, using the same
production source files.

## Schema 2 / feature schema 3 (firmware 0.3.0)

The 28-byte frame and CRC stay unchanged. Converter preserves all fields/types
without relabeling old captures. Replay now displays selected branch windows.
All window boundaries are microseconds, half-open [start,end).

| Type | timestamp_us | sequence | value_a | value_b | flags |
|---|---|---|---|---|---|
| 38 PPG window | start | duration_us | quality reasons | received samples | quality status low byte; Stress feature status high byte |
| 39 ECG window | start | duration_us | quality reasons | received samples | quality status low byte; ECG feature status high byte |
| 40 SpO2 window | start | duration_us | PPG60 quality reasons | retained source samples | PPG60 quality status low byte; estimator status high byte |
| 41 Rhythm | result time | inference reasons | P(AF) float bits | confidence float bits | inference status low byte; label high byte |
| 42 Stress | result time | inference reasons | P(Stress) float bits | confidence float bits | inference status low byte; label high byte |
| 43 Feature counts | result time | Stress beats | raw intervals low16, clean intervals high16 | ECG valid intervals | feature schema |
| 44 PPG integrity | result time | expected | received | dropped lower bound | reserved |
| 45 ECG integrity | result time | expected | received | observed discontinuities | reserved |

Values such as probability are meaningful only when the associated status is Valid.
The SpO2 gate is the conservative PPG60 gate, not independent 4-second SQI.
Labels in `types/result_types.h`: None=0, Baseline=1, Stress=2, AF=3, non-AF=4.
Inference status: Valid=0, InvalidInput=1, NotReady=2, QualityRejected=3, Error=4.
Feature status: NotReady=0, Ready=1, QualityRejected=2, InvalidInput=3.

## Verification tools

From repository root:

```powershell
python firmware/tools/host_inference/run.py --output ecg-host-results.json
python firmware/tools/serialization_test/run.py
python docs/ai-compatibility/evidence/run_checks.py --reference ../NCKH_AI_Modules-reference --feature-oracle --output audit-results.json
```

The first command compiles the actual pinned ECG SDK/export with host platform
services, bypassing neither classifier nor standard scaler. It uses four compiler
workers and temporary ASCII paths. Output is a synthetic-vector smoke test with
source hash, not independent clinical/golden validation. Output quantization is
1/256; a saturated softmax sum of 255/256 is expected, not normalized away.

The second command compiles the production binary logger and MQTT serializer with
fake transports, validates schema/labels/windows/probabilities and CRC rejection.
It neither contacts a broker nor opens a serial port. The feature oracle option
in the third command requires NumPy; the other tooling requires Python and g++.
