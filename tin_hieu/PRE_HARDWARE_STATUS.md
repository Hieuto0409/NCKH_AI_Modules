# Trạng thái chuẩn bị trước khi có phần cứng — v0.4

## Đã chuẩn bị và không đổi theo cảm tính

- Biquad causal filter và cách tính hệ số theo `fs_hz`.
- Hai nhánh ECG, bốn nhánh PPG AC/DC.
- Ring buffer 5 s, step 2.5 s và thống kê cửa sổ.
- Perfusion, ECG RMS gate, `both_good` và `quality_reason_mask`.
- Timestamp/gap diagnostics.
- Causal peak detector có polarity/look-ahead/refractory/prominence.
- RR/PPI, HR/PR, SDRR/SDPPI, RMSSD không bắc cầu interval invalid.
- Host smoke tests, deterministic peak/feature tests, strict compiler flags.
- Serial RAW_ONLY/FULL modes và parser capture không phụ thuộc prefix.
- Driver skeleton MAX30102/AD8232 dạng Arduino C++ có guard placeholder, không hard-code chân
  ESP32-S3 chưa xác minh.

## Chỉ chốt sau khi đo MAX30102 + AD8232 thật

1. `fs_hz` thực tế của từng luồng và quy ước resampling về một processing fs.
2. Timestamp monotonic, dropped samples và FIFO scheduling.
3. Slot FIFO nào là LED đỏ/IR; firmware dùng `red_raw`/`ir_raw`, không dùng
   `pleth_1`/`pleth_2`.
4. ADC offset/range của AD8232 và scale/count của MAX30102.
5. `PERFUSION_MIN_RED`, `PERFUSION_MIN_IR` và ECG RMS floors.
6. Peak polarity/prominence/refractory trên waveform thật.
7. RAM/CPU budget khi bật Wi-Fi, OLED và model AI.
8. Quy tắc finger-off/motion recovery và ngưỡng reject cuối.

## Tiêu chí hoàn tất hardware validation

- Có raw CSV chứa `timestamp_us`, `sample_index`, ECG, Red, IR và dropped count.
- Có báo cáo `fs_median_hz`, dt min/median/max, gaps, range/std mỗi kênh.
- Có ít nhất các phiên finger-off, finger-rest, finger-tilt và motion nhẹ.
- Replay raw CSV bằng Python và đối chiếu waveform/perfusion/quality với C.
- Mọi ngưỡng đã đổi được ghi vào một config version mới; không ghi đè reference
  offline v0.4.

## Ranh giới với AI

Firmware/reference chỉ xuất filtered signal, quality flags và feature hợp lệ.
AI teammate chịu trách nhiệm model, split theo subject, chuẩn hóa feature và
nhãn stress/WESAD. Không đưa EDA vào v0.4 này; EDA là future work riêng.
