# SignalProcessor reference v0.4 — Arduino IDE / ESP32-S3

## Mục đích

Đây là module C tham chiếu cho phần xử lý tín hiệu của Task 2. Nó được chuẩn
bị để compile/test trước khi có MAX30102 và AD8232 thật, rồi thay phần dummy
input bằng driver phần cứng.

Đây chưa phải firmware y tế và chưa phải bằng chứng độ chính xác y khoa.

## Đã có trong gói

- Biquad IIR Direct Form II Transposed, hệ số thiết kế runtime theo `fs_hz`.
- Sáu nhánh lọc realtime:
  - ECG morphology: 0.5–35 Hz.
  - ECG QRS: 5–20 Hz.
  - PPG Red AC và IR AC: 0.5–8 Hz.
  - PPG Red DC và IR DC: low-pass 0.5 Hz.
- Ring buffer đúng cho cửa sổ trượt 5 s / bước 2.5 s.
- Perfusion Red/IR.
- SQI Mức A: PPG theo perfusion; ECG theo RMS floor placeholder.
- Timestamp diagnostics: phát hiện gap và lưu `gap_count`.
- `quality_reason_mask` và `window_gap_count` cho firmware/AI gate.
- `PeakDetector.c/.h`: peak detector causal có polarity, prominence,
  refractory và look-ahead trễ cố định.
- `FeatureExtractor.c/.h`: RR/PPI, HR/PR, SDRR/SDPPI và RMSSD không bắc cầu
  qua interval invalid.
- `FORMULAS.md`: công thức và quy ước bất biến.
- `HARDWARE_TUNING.md`: danh sách duy nhất các điểm cần hiệu chuẩn khi có board.
- `max30102.cpp/.h`, `ad8232.cpp/.h`: lớp Arduino/Wire driver skeleton có
  kiểm tra placeholder và TODO phần cứng; chưa coi là driver đã validate.
- `HARDWARE_INTEGRATION.md`: trình tự I²C/FIFO/ADC và test khi board về.
- Sketch dummy phát CSV qua Serial.
- Công cụ Python để ghi và phân tích CSV.

## Giới hạn cần ghi rõ

- `DEFAULT_FS_HZ = 100` chỉ để compile/dummy test.
- Khi có phần cứng, đo fs thực tế rồi gọi `sp_init(&processor, fs_thuc_te)`.
- Ngưỡng Red `0.0005` và IR `0.0002` là tham chiếu offline; cần kiểm tra lại
  trên MAX30102 thật.
- ECG RMS floor chưa tương đương đầy đủ `low_qrs_power` của Python.
- Peak detector/feature extractor là reference causal, chưa phải detector y tế
  và chưa bit-exact với Python.
- Chưa có PAT/PTT lâm sàng, SpO2 đã hiệu chuẩn hoặc model AI.
- Bandpass C dùng HP+LP cascade để chạy realtime, không bit-exact với
  `scipy.signal.butter`/`sosfiltfilt` trong Python.

## Compile bằng Arduino IDE khi chưa có board

1. Mở `tin_hieu.ino`.
2. Chọn `Tools → Board → esp32 → ESP32S3 Dev Module`.
3. Chọn `Flash Size: 16MB`, `PSRAM: OPI PSRAM` nếu các mục này xuất hiện.
4. Không cần chọn Port khi chỉ bấm Verify.
5. Bấm Verify.

Arduino IDE tự biên dịch các file `.c`/`.cpp` ở cùng thư mục sketch. Giữ
`SignalProcessor.c`, `PeakDetector.c`, `FeatureExtractor.c` là C thuần; driver
Arduino dùng `.cpp` vì gọi `Wire`/`analogRead`.

`SERIAL_STREAM_RAW_ONLY` là mặc định để không làm nghẽn UART ở fs cao. Chế độ
`FULL` chỉ dùng bench/debug khi băng thông đủ. `tools/capture_serial.py` tự đọc
cả hai prefix (`S,` và `SAMPLE,`) nhưng lưu một CSV raw replay ổn định gồm
timestamp, sample index, ECG/Red/IR, window flag và dropped-sample counter.

## Test trước phần cứng

`USE_DUMMY_INPUT` trong `tin_hieu.ino` đang bằng `1`. Sketch tạo tín hiệu giả
lập và in các dòng `S,...` ở chế độ mặc định (`FULL` dùng `SAMPLE,...`).

- `red_perf`/`ir_perf` xuất hiện trong các dòng `WINDOW,...`.
- `quality_reason_mask` dùng các bit trong `SignalProcessor.h`; bit 0/1 là
  perfusion Red/IR thấp, bit 2/3 là ECG RMS thấp, bit 4 là timestamp gap,
  bit 5 là cấu hình bị clamp.
- Cửa sổ đầu tiên xuất hiện sau 5 s, sau đó mỗi 2.5 s.
- Test này chỉ kiểm tra luồng phần mềm.

## Khi có phần cứng

### Bước 1 — Chốt chân và driver

Đặt `USE_DUMMY_INPUT = 0` chỉ sau khi đã kiểm tra schematic ESP32-S3 N16R8.
Thay các giá trị `255` trong `config.h` bằng chân I²C, ADC ECG và LO+/LO− thực
tế. Không lấy GPIO 34/35/36 theo ví dụ trên mạng nếu chúng đang dành cho
Flash/PSRAM của module.

Driver skeleton đã có sẵn nhưng vẫn phải xác nhận part ID, FIFO order, LED
current, pulse width và sample rate trên module thật.

### Bước 2 — Đo và log raw

Firmware teammate thay `read_hardware()` bằng code đọc:

- `ecg_raw` từ AD8232 ADC.
- `red_raw` và `ir_raw` từ hai slot FIFO MAX30102.
- `timestamp_us = micros()`.

Không giả định `pleth_1/pleth_2` trên phần cứng. MAX30102 phải đặt tên rõ
theo cấu hình LED/FIFO thực tế là `red_raw` và `ir_raw`.

### Bước 3 — Ghi CSV trên máy tính

Cài công cụ:

```bash
python -m pip install -r tools/requirements.txt
```

Sau khi cắm board và xác định COM:

```bash
python tools/capture_serial.py \
  --port COM13 --baud 921600 \
  --out raw_hw_s01_sit.csv --seconds 120
```

Đổi `COM13` thành Port thực tế. Không đoán Port khi chưa cắm board.

### Bước 4 — Kiểm tra sampling rate và gap

```bash
python tools/analyze_capture.py raw_hw_s01_sit.csv
```

Ghi lại:

- số mẫu và thời lượng;
- `fs_median_hz`;
- `dt_min_us`, `dt_median_us`, `dt_max_us`;
- số gap;
- min/max/std của ECG, Red, IR.

### Bước 5 — Cập nhật bộ lọc

Sau khi biết fs thật, firmware phải gọi:

```cpp
sp_init(&processor, fs_thuc_te);
```

Không sửa cutoff tùy tiện trước khi có log. Các hệ số biquad được tính lại
trong `sp_init()` theo fs mới.

### Bước 6 — Test tối thiểu

Thực hiện từng điều kiện và lưu file riêng:

1. Không đặt ngón tay — kiểm tra PPG poor.
2. Đặt ngón tay yên — kiểm tra PPG good.
3. Tháo/nghiêng ngón tay — kiểm tra low perfusion.
4. ECG điện cực đúng vị trí — kiểm tra ECG waveform.
5. Cử động nhẹ — kiểm tra gap/SQI.
6. Ít nhất 5 người khỏe mạnh cho engineering test; không gọi là clinical
   validation.

## Peak và feature sau khi lọc

Firmware có thể đưa `result.ecg_qrs` hoặc `result.ecg_filtered` vào
`PeakDetector`, đưa `result.red_ac` (hoặc IR AC theo mục tiêu) vào detector PPG,
rồi gọi `fx_add_ecg_peak`/`fx_add_bvp_peak`. Khi đủ peak, gọi `fx_compute` để
nhận `FeatureSet`. Việc gán peak vào cửa sổ 5 s/2.5 s là trách nhiệm của lớp
window manager; không reset accumulator tùy tiện giữa hai cửa sổ chồng lấn.

Các ngưỡng prominence trong `config.h` chỉ là starting point cho dummy. Khi có
ADC thật, tune theo raw capture và ghi version config.

## So sánh với Python reference

CSV raw từ phần cứng là dữ liệu cần gửi lại cho Task 2. Chạy pipeline Python
trên cùng dữ liệu sau khi xác nhận sampling rate và mapping. So sánh:

- waveform sau lọc;
- perfusion Red/IR;
- tỷ lệ cửa sổ good/poor;
- gap và số cửa sổ;
- nếu sau này thêm peak detection: BPM/IBI.

Sai khác nhỏ là bình thường vì bộ lọc C là causal realtime, còn Python có thể
dùng xử lý offline hai chiều. Không điều chỉnh ngưỡng để ép kết quả giống nhau
mà không ghi lại lý do.

## Handoff cho firmware

Firmware nhận `SignalProcessor.c/.h`, `biquad.c/.h`, `PeakDetector.c/.h`,
`FeatureExtractor.c/.h`, `max30102.cpp/.h`, `ad8232.cpp/.h`, `config.h` và hướng
dẫn này. Firmware tích hợp driver sensor ở `tin_hieu.ino`, sau đó nối
`both_good`/features vào đúng input contract của model AI. Sau mỗi lần gọi
`ad8232_read_sample()`, firmware cũng phải đọc `ecg_state.leads_off` để gắn
quality flag; hàm vẫn trả mẫu ADC hợp lệ và không tự tạo timestamp gap.
