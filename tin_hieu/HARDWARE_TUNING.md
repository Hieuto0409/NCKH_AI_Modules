# Hardware tuning checklist

Mục tiêu là khi có board thật chỉ sửa các mục ở bảng dưới. Công thức và module
DSP không sửa theo cảm tính; mọi thay đổi phải kèm raw CSV và lý do.

## Được phép/ cần đổi sau khi có phần cứng

| Hạng mục | Chỗ thay đổi | Cách chốt |
|---|---|---|
| Driver AD8232 ADC | `tin_hieu.ino`/HAL | Đọc đúng chân, range, offset; không đưa GPIO code vào `SignalProcessor.c`. |
| Driver MAX30102 FIFO | `tin_hieu.ino`/HAL | Xác nhận slot LED đỏ và IR; xuất tên `red_raw`, `ir_raw`. |
| Tần số ECG/PPG | acquisition + `sp_init(fs)` | Tính từ timestamp thật, không lấy mặc định 100/500 Hz. |
| Timestamp/sync | acquisition | Ghi `timestamp_us`, `sample_index`, dropped samples; xử lý rate khác nhau bằng resampling/buffering đã thống nhất. |
| ADC/LED scaling | driver/config | Ghi cả raw integer và đơn vị float; không so threshold ADC giữa hai sensor khác nhau. |
| Perfusion thresholds | `config.h`/config runtime | Hiệu chuẩn trên raw capture thật; Red/IR riêng. |
| ECG RMS floors | `config.h`/config runtime | Đo baseline + biên độ AD8232 thật; giữ guard an toàn. |
| Peak polarity/prominence | cấu hình `PeakDetector` | Xem waveform thật; chọn polarity rõ ràng, không dùng `abs` mù. |
| Buffer budget | `MAX_WINDOW_SAMPLES`, peak cap | Tính RAM với Wi-Fi/BLE/model đã bật; không vượt giới hạn. |

## Không được đổi tùy tiện

- Không đổi công thức perfusion, RR/PPI, SDRR hoặc RMSSD chỉ để tăng tỷ lệ
  `good`.
- Không gọi `pleth_1/pleth_2` trong firmware MAX30102; đó là tên cột dataset.
- Không copy ngưỡng PhysioNet sang WESAD BVP 64 Hz.
- Không coi `both_good` là accuracy hoặc medical validation.
- Không bỏ raw log sau khi đã có feature.

## Quy trình khi board về

1. Compile/flash dummy sketch trước, rồi thêm driver từng sensor một.
2. Thu ít nhất 30--120 s ở các trạng thái: finger-off, finger-rest, finger-tilt,
   ECG điện cực đúng, cử động nhẹ.
3. Chạy `tools/analyze_capture.py` để lấy `fs_median_hz`, dt và gap.
4. Xác nhận Red/IR FIFO bằng cấu hình MAX30102 và waveform; ghi vào log/config.
5. Chạy lại pipeline Python trên **raw capture**, so waveform/perfusion/quality.
6. Chỉ sau bước 5 mới chốt threshold và peak parameters cho firmware.
7. Lưu version config cùng firmware và raw CSV; mọi kết luận chỉ là engineering
   validation, chưa phải clinical validation.

## Input contract cho firmware teammate

Mỗi mẫu sau driver/resampling phải cung cấp:

```text
timestamp_us : uint32_t, monotonic
sample_index : uint32_t, monotonic
ecg_raw      : float
red_raw      : float
ir_raw       : float
dropped_samples : uint32_t (nếu driver biết)
```

Firmware gọi `sp_process_sample(...)`, sau đó có thể đưa `ecg_filtered` và
`red_ac`/`ir_ac` vào `PeakDetector`, thêm peak vào `FeatureAccumulator`, và chỉ
gửi `FeatureSet` khi `feature_valid` cùng quality flags đạt.
