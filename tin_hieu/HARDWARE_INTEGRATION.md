# Hardware integration checklist

Đây là checklist nối driver, không phải cam kết rằng các chân dưới đây đã đúng
cho board cụ thể. Với ESP32-S3 N16R8, phải kiểm tra schematic/module pinout và
không dùng chân đang nối Flash/PSRAM.

## Trình tự

1. Chạy I²C scanner, xác nhận MAX30102 ACK ở `0x57`.
2. Chốt chân I²C, ADC ECG và LO+/LO− trong `config.h`; không để giá trị `255`.
3. Xác nhận part ID, FIFO order, LED current, pulse width và sample-rate của
   MAX30102. Driver skeleton dùng 100 Hz/411 µs chỉ làm giá trị khởi đầu.
4. Đọc FIFO theo dữ liệu có sẵn; không đọc tràn FIFO và không tạo sample giả.
5. Đọc ECG cùng timestamp processing; nếu hai sensor có rate khác nhau phải
   dùng buffering/resampling có chủ đích trước `sp_process_sample()`.
6. Thu raw CSV ở finger-off, finger-rest, finger-tilt, ECG leads-on và motion.
7. Chạy `tools/analyze_capture.py`, sau đó replay/đối chiếu bằng Python.

## Driver files

- `max30102.h/.cpp`: I²C, part ID, FIFO RED/IR, overflow counter. LED1 được đặt
  tên `red_raw`, LED2 được đặt tên `ir_raw`; xác nhận lại trên module thật.
- `ad8232.h/.cpp`: ADC ECG và leads-off. Leads-off là quality state, không làm
  mất timestamp bằng cách bỏ sample.

## Không được chốt từ skeleton

- GPIO cụ thể.
- LED current/pulse width/sample-rate tối ưu.
- Threshold perfusion/ECG RMS.
- Tuyên bố tín hiệu sạch hoặc độ chính xác y khoa.
