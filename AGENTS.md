# Guidance for Codex

## Repository layout

The user requested the repaired integrated firmware in this repository:
`https://github.com/Hieuto0409/NCKH_AI_Modules`.

- `firmware/` is the integrated device firmware 0.5.0. Build and test there.
- `tin_hieu/` is the standalone signal-processing reference, not a linked library
  in the integrated firmware. Do not assume its combined-rate API matches both streams.
- Root `src/`, `include/`, `lib/`, `tools/`, `test/`, and `platformio.ini` remain
  the existing AI modules/demo. Do not overwrite them when changing device firmware.

## Current OLED/ThingsBoard behavior (0.5.0)

Read `docs/thingsboard-oled/FLOW.md` and `VALIDATION.md` first. OLED result pages
are metrics, AI screening, upload; BTN2 changes pages and retries pending data in
Idle. Keep OLED asleep during warmup/acquisition. The Internet ThingsBoard profile
uses HTTPS 443 with GTS Root R4, hostname/time verification and HTTP 200 ACK;
the older TCP MQTT profile is for trusted LAN only. Never embed tenant login or
commit device tokens, Wi-Fi credentials, or firmware binaries containing them.
Distinguish verified host-to-server connectivity from ESP32-to-server validation.

## Power/network baseline (0.4.0, preserved in 0.5.0)

Read `docs/power-and-noise/REVIEW.md` and `VALIDATION_RESULTS.md` for the schematic
review and post-measurement upload lifecycle. Wi-Fi must remain off throughout
contact/warmup/acquisition. Keep bounded uploads, QoS1/HTTP200 acknowledgement, no silent
queue loss, sensor pause/resume, and unmodified sample rates/model contracts.
Do not claim measured battery life or eliminated interference from host tests.

Read `docs/board-validation/2026-09-27.md` for subsequent physical-board testing
and final installed offline production profile. AI fixture, short acquisition,
sensor pause/resume and timer sleep ran on ESP32-S3. DS3231 at 0x68 NACK and one
ECG dropped-slot count per start remain open. Do not equate fixture execution
with clinical validation or short diagnostic sleep with GPIO/production validation.
Always restore the production profile after `esp32-s3-devkitc-1-board-diag`.

## Read before changing firmware

Read `docs/ai-compatibility/REPOSITORY_HANDOFF.md`, `REPAIR_REPORT.md`,
`IMPLEMENTATION_PLAN.md`, `VALIDATION.md` in that directory, then
`firmware/lib/PROVENANCE.md`. Baseline defect descriptions in the plan are
historical; the 0.3.0 software repairs are implemented. Hardware validation and
independent ECG golden results remain open. Check current source and Git status.

Preserve the ESP32-S3 N16R8 pinout, MAX30102 RED/IR 200 Hz average 1 with polling
(no interrupt connected), and ECG ADC1 target 500 Hz. Keep the model pin
`7d4683581c8c01ff6878bcad31a601aff271f2a0` unless a model upgrade is requested.
Preserve generated SDK/model files and license notices.

Stress uses 14 ordered features from 60 s PPG; ECG uses 9 ordered features from
a selected 30 s ECG window. SpO2 uses raw RED/IR with the 200-to-100 Hz adapter.
Do not weaken SQI, conceal missing samples, fabricate past ADC samples, or enable
unvalidated LowO2 rules. `non-AF` does not imply generally normal rhythm.

Native tests use an ECG stub; actual host inference is a separate tool under
`firmware/tools/host_inference/`. Keep software checks distinct from board and
clinical validation. Follow `VALIDATION.md` for behavior changes and record
remaining unrun checks accurately.
