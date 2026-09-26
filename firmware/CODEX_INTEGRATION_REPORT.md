# BÁO CÁO TÍCH HỢP NCKH_AI_MODULES

> Cập nhật sửa 2026-09-26: firmware hiện là **0.3.0**. Đã sửa acquisition,
> clock/warmup và cửa sổ ECG; xem [REPAIR_REPORT.md](../docs/ai-compatibility/REPAIR_REPORT.md)
> cho bằng chứng hiện hành và phần kiểm chứng phần cứng còn mở.
> Nội dung từ mục 1 bên dưới được giữ như **lịch sử tích hợp 0.2.0**, không phải
> kết luận về phiên bản mới. Native bypass ECG được bổ sung bằng host inference thật.

Ngày chốt: 2026-09-26 (Asia/Bangkok)

## 1. Baseline và provenance

- Workspace hiện tại không phải Git checkout, vì vậy không có baseline commit để
  báo cáo. Baseline firmware trước sửa là `0.1.0`; bản tích hợp này là `0.2.0` với
  feature schema `2` và SpO2 algorithm version `2`.
- Repo model/numerical contract: `Hieuto0409/NCKH_AI_Modules`.
- Commit được pin và đối chiếu trực tiếp:
  `7d4683581c8c01ff6878bcad31a601aff271f2a0`.
- Các thư mục vendored: `lib/Stress_PPG_60s_model`, `lib/ResearchSpO2` và
  `lib/ECG_arrhythmia_EI`. Không copy `src/main.cpp` của repo AI.
- License header sinh bởi Edge Impulse được giữ nguyên. Header nêu rõ việc sử dụng
  cần gói Edge Impulse trả phí đang hoạt động và đủ điều kiện. Phải xác nhận quyền
  sử dụng trước khi phân phối hoặc triển khai.

## 2. Kết quả audit baseline

- Pinout trong `include/config/pins.h` đã khớp FINAL SCHEMATIC: PPG 12/13; ECG
  OUT/LO+/LO- = 1/5/6; battery 8; OLED 38-42; BTN1/BTN2 = 48/47.
- PPG baseline đã là 200 Hz, average 1, Red+IR, pulse width 411 us, polling/drain
  FIFO. ECG baseline cấu hình 500 Hz trên common monotonic timebase.
- Native baseline: 16/16 test qua.
- Build firmware baseline trực tiếp trong đường dẫn Unicode thất bại ở Xtensa với
  lỗi đường dẫn tạm `Invalid argument`. Script Windows cũ cũng không tìm được `pio`
  và cleanup không an toàn. Đây là lỗi môi trường build, không phải lỗi C++.
- AI engine baseline chưa nối model thật; Stress/Rhythm/Low-O2 trả trạng thái chưa
  sẵn sàng.

## 3. Kiến trúc sau tích hợp

```text
MAX30102 Red/IR 200 Hz
  |-- integrity + SQI + PPG peak/PPI
  |      `-- StressPpg60sAdapter (rolling 60 s, 14 features)
  |             `-- stress_ppg_model.h
  `-- Spo2RateAdapter200To100 (boxcar 2 mẫu, deterministic)
         `-- ResearchSpO2::Stream100 (400 pairs / 4 s)

AD8232 ECG 500 Hz
  `-- integrity + SQI + R-peak/RR
         `-- EcgAfFeatureAdapter (9 features, seconds/%/ratio)
                `-- Edge Impulse AF / non-AF

3 branch results + validity/quality
  `-- DecisionAggregator
         `-- không Normal nếu branch thiếu/quality fail;
             non-AF không được dùng làm bằng chứng Rhythm normal
```

`src/main.cpp` vẫn chỉ bootstrap. Fixture tổng hợp chỉ được chọn bằng macro
`PPGFW_OFFLINE_FIXTURE` trong environment riêng và luôn in banner
`OFFLINE TEST — NOT A SENSOR MEASUREMENT`. Default environment không tạo dữ liệu giả.

## 4. Contract đã hiện thực

### Stress PPG

- Rolling window riêng 60 s, tối thiểu 40 beat.
- Lọc PPI 333,33-1500 ms, median ±30%, valid ratio tối thiểu 0,65 theo repo.
- Đúng 14 feature và thứ tự; bpm/ms/%/ratio/count; sample standard deviation khi
  contract dùng `ddof=1`.
- Chỉ inference khi PPG-SQI tốt và feature QC sẵn sàng.
- Fixture xác suất khớp reference repo: `0.648799505354633710` trong tolerance
  native float `1e-6`.
- Con số 93,33% chỉ là WESAD S13+S16 evaluation, không phải độ chính xác MAX30102.

### ECG AF/non-AF

- Đúng 9 feature theo fusion axes: `mean_rr`, `median_rr`, `sdnn`, `rmssd`,
  `pnn50`, `cv_rr`, `iqr_rr`, `min_rr`, `max_rr`.
- Các interval feature dùng giây; pNN50 dùng phần trăm; cv_rr không thứ nguyên;
  percentile dùng nội suy tuyến tính tương thích NumPy.
- Target build gọi trực tiếp `process_impulse` của model pin, đồng thời kiểm tra
  frame size, axes và labels `AF` / `non-AF`.
- Nhánh này chỉ là AF screening. `non-AF` có `negative_conclusive=false`, nên không
  thể tự tạo kết quả product-level `Normal`.

### SpO2

- Global acquisition vẫn 200 Hz.
- Adapter lấy trung bình từng cặp synchronized Red/IR liên tiếp, tạo đúng 100 Hz.
  Đây là boxcar FIR hai mẫu trước khi giảm tần số; 800 source samples tạo 400 pairs.
- Reset stream khi sequence/timestamp gap, FIFO overflow/dropout hoặc thay đổi
  sample-rate/LED/ADC config.
- External quality đến từ PPG quality gate độc lập.
- `ResearchSpO2::Status::Ok` chỉ map thành software value hợp lệ;
  `clinically_validated` luôn `false` ở phiên bản này.

### Logging và UI/network

- Log firmware/schema/model/scaler version, commit AI pin, reason flags và inference
  latency; binary record mới là 19 (provenance) và 37 (latency).
- MQTT mang commit, model/scaler version, latency, rhythm label và cờ
  `spo2_clinically_validated`.
- OLED ghi `SpO2 est`; màn hình ECG giữ semantic AF/non-AF và không trình bày
  non-AF là Normal.

## 5. File thêm/thay đổi

Nhóm file chính (vendor directory liệt kê theo nhóm thay vì 1.384 file):

- Thêm `include/config/model_contracts.h`, `include/types/model_feature_types.h`.
- Thêm `features/stress_ppg_60s_adapter.*`, `features/ecg_af_feature_adapter.*`.
- Thêm `metrics/spo2_rate_adapter.*`, `ai/ecg_af_model_adapter.*`.
- Thêm `app/offline_fixture_app.*` và hai test suite
  `test/test_model_adapters`, `test/test_spo2_rate_adapter`.
- Thêm `lib/PROVENANCE.md`, vendored `lib/Stress_PPG_60s_model`,
  `lib/ResearchSpO2`, `lib/ECG_arrhythmia_EI`.
- Sửa acquisition continuity trong `acquisition/ppg_acquisition.cpp` và
  `acquisition/ecg_acquisition.cpp`.
- Sửa `feature_builder`, ba AI engines, decision aggregator, result/metric/feature
  contracts, logging, telemetry, OLED, versions/sampling, `main.cpp`, tests hiện có,
  `platformio.ini` và `tools/build_windows.ps1`.

## 6. Lệnh kiểm tra và kết quả

Native tests:

```powershell
& 'C:\Users\HELIOS NEO 16\.platformio\penv\Scripts\platformio.exe' test -e native
```

Kết quả cuối: 8 suites, 26 test cases, 26 passed.

Default firmware:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_windows.ps1 -Environment esp32-s3-devkitc-1
```

Kết quả: SUCCESS; RAM 74.428/327.680 bytes (22,7%); flash
400.649/6.553.600 bytes (6,1%); application image 401.008 bytes.

Explicit offline fixture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_windows.ps1 -Environment esp32-s3-devkitc-1-fixture
```

Kết quả: SUCCESS; RAM 22.264/327.680 bytes (6,8%); flash
329.961/6.553.600 bytes (5,0%); application image 330.320 bytes.

Generated Edge Impulse SDK phát cảnh báo redefine port-selection macro và một cảnh
báo API FlatBuffers deprecated. Không có warning từ adapter/model contract mới và
không có build error. Generated files không bị sửa để che cảnh báo.

## 7. Giới hạn và validation còn mở

- Chưa có board/sensor trong môi trường chạy này, nên chưa đo inference latency,
  heap/stack/PSRAM runtime, acquisition jitter/dropout hay FIFO stress thực. Firmware
  đã log latency và diagnostics để đo trên board.
- Chưa chạy fixture binary trên ESP32-S3; hiện đã xác minh bằng target compile/link
  và native numerical tests. Cần flash environment fixture và lưu serial output.
- Cần bench-test MAX30102 LED/ADC/contact/SQI/peak tại 200 Hz, và kiểm chứng adapter
  SpO2 với raw capture thực cùng reference độc lập.
- SpO2 chưa calibration/clinical validation; không được gọi medical-grade.
- Cần bench-test AD8232 voltage range, lead-off, filter, R-peak và ECG 500 Hz timing;
  AF model không thay thế detector mọi loại rối loạn nhịp.
- Nhánh Low-O2 rule vẫn `NOT_READY` vì threshold chưa được validation. Do đó firmware
  cố ý không suy ra `Normal` toàn cục từ SpO2/AF hiện tại.
- Cần power/battery safety validation cho divider và cấu hình 1S2P, cùng soak test
  tối thiểu 30 phút.
- Hai artifact MQTT/raw-log cũ trong `dist/` chưa rebuild cho 0.2.0; phải build lại
  profile tương ứng trước khi sử dụng.

## 8. Deviation so với repo contract

Không có deviation ở feature order, units, label hoặc model internals. Điểm tích hợp
chủ đích là firmware giữ PPG global 200 Hz và dùng boxcar two-sample adapter để cấp
100 Hz cho ResearchSpO2, đúng quyền ưu tiên của system specification. Các ngưỡng QC
mới chỉ lấy từ repo pin và được centralize; không thêm ngưỡng y sinh để làm test pass.
