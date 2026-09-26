# Vendored AI module provenance

Source repository: `Hieuto0409/NCKH_AI_Modules`

Pinned commit: `7d4683581c8c01ff6878bcad31a601aff271f2a0`

Vendored modules:

- `Stress_PPG_60s_model/stress_ppg_model.h`: 14-feature StandardScaler and logistic-regression inference contract.
- `ResearchSpO2`: `Stream100` and Maxim algorithm implementation.
- `ECG_arrhythmia_EI`: Edge Impulse AF/non-AF model, generated SDK, model metadata, compiled model and original license headers.

The Edge Impulse generated files state that use requires an eligible active paid Edge Impulse subscription and continued compliance with the applicable Edge Impulse license. Those headers are preserved unchanged. Confirm licensing before distributing or deploying the firmware.

The Stress result reported as 93.33% is the evaluation-model result on the WESAD S13 and S16 test split. It is not accuracy on MAX30102. ResearchSpO2 `Status::Ok` is software acceptance of a window and is not clinical validation.
