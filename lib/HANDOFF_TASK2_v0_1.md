# HANDOFF TASK 2 – ECG/PPG Signal Processing & SQI v0.1

## 1. Kết luận bàn giao

Task 2 đã hoàn tất gói xử lý tín hiệu và SQI ở mức **engineering baseline v0.1**. Gói này tạo đầu vào ECG/PPG sạch hơn, kèm cờ chất lượng để tầng AI chỉ sử dụng các cửa sổ có độ tin cậy phù hợp. Task 2 không phụ trách gán nhãn lớp AI, chọn dataset, train hoặc đánh giá model.

## 2. Cấu trúc cần giữ riêng

### A. Hardware capture và SQI

Thư mục: `step2_handoff_sqi_v0_1/`

- `raw/`: bốn file raw gốc, không chỉnh sửa.
- `sqi_calibration_v01/`: script hiệu chỉnh SQI, các bảng cửa sổ và `sqi_thresholds_v0_1.json`.
- `offline_step3_v01/`: các file ECG/PPG đã xử lý, cửa sổ SQI, `offline_summary.json` và preview.
- `README_HANDOFF.md`: mô tả gói hardware.

Kết quả kiểm tra hiện tại:

- ECG thực tế khoảng 500 Hz; PPG thực tế khoảng 25 Hz.
- Không có missing sample, index regression hoặc timestamp regression.
- ECG sạch dài: 22/23 cửa sổ tốt.
- ECG nhiễu: 0/23 cửa sổ tốt.
- ECG sạch ngắn: 3/3 cửa sổ tốt.
- PPG: 23/23 cửa sổ tốt, FIFO hợp lệ 100%.

Phân loại các file raw trong `01_hardware_sqi/step2_handoff_sqi_v0_1/raw/`:

| File | Loại | Trạng thái |
|---|---|---|
| `ecg_raw(8).csv` | ECG sạch dài, khoảng 60 giây | Tốt: 22/23 cửa sổ SQI đạt |
| `ecg_raw(5).csv` | ECG sạch ngắn, khoảng 10 giây | Tốt: 3/3 cửa sổ SQI đạt |
| `ecg_raw(9).csv` | ECG có nhiễu | Xấu/reject: 0/23 cửa sổ SQI đạt |
| `ppg_raw(5).csv` | PPG tham chiếu, khoảng 60 giây | Tốt: 23/23 cửa sổ đạt, FIFO 100% |

Nếu Windows tự đổi tên thành `ecg_raw(8)(1).csv`, `ecg_raw(5)(1).csv`... thì đó chỉ là tên bản sao; cần kiểm tra nội dung, không dùng tên `(1)` để suy ra chất lượng.

### B. Embedded signal processing

Giữ trong thư mục code xử lý tín hiệu hiện tại, gồm các module như:

- `SignalProcessor.c/.h`
- `FeatureExtractor.c/.h`
- `PeakDetector.c/.h`
- `max30102.*`
- `config.h`
- file `.ino`

Đây là phần để firmware port pipeline raw → filter → window → SQI → feature. Capture firmware vẫn raw-only; không filter, resample hoặc normalize trước khi ghi raw.

### C. PhysioNet/offline research

Giữ riêng thư mục PhysioNet/offline gồm `pipeline.py`, `process_ptt.py`, `src/`, `config/`, `data/` và `tests/`.

PhysioNet dùng để nghiên cứu, replay và kiểm tra thuật toán. Không gộp trực tiếp với hardware raw nếu chưa thống nhất sampling rate, tên cột, windowing và preprocessing.

## 3. Phân công

### Task 2 – người bàn giao

- Kiểm tra tính toàn vẹn raw và sampling rate.
- Filter ECG/PPG.
- Peak detection, IBI/PPI, BPM/HRV/PRV và perfusion feature.
- Tính SQI và quality flags: `ecg_good_v01`, `ppg_good_v01`.
- Chốt filter, cửa sổ, threshold, feature schema và replay test.
- Bàn giao spec/config cho firmware và AI.

### Firmware

- Đọc cảm biến và capture raw.
- Giữ giao thức ECG/PPG raw hiện tại.
- Port các filter, window, SQI và feature đã chốt.
- Replay dữ liệu CSV để so sánh C/C++ với Python.

### AI

- Chọn dataset và nguồn dữ liệu.
- Gán nhãn lớp AI.
- Chia train/validation/test.
- Train, đánh giá và tối ưu model.

Các cờ SQI chỉ là **quality flags**, không phải nhãn bệnh.

## 4. Giới hạn cần ghi rõ

- `slot0_raw` và `slot1_raw` chưa được đổi tên thành Red/IR nếu chưa xác nhận mapping MAX30102.
- `finger_contact_available=false` trong capture hiện tại.
- SpO2 calibration và đánh giá lâm sàng thuộc phần mở rộng của teammate phụ trách SpO2, không phải deliverable chính của Task 2.
- BPM/HR trong v0.1 là output nội bộ để replay/demo, không dùng làm tuyên bố độ chính xác lâm sàng.
- Mọi thay đổi về filter, mapping slot, sampling rate hoặc threshold phải tạo phiên bản mới, ví dụ v0.2; không ghi đè v0.1.

## 5. Thứ tự gửi cho hai bạn

1. Gửi toàn bộ `step2_handoff_sqi_v0_1/` cho cả firmware và AI.
2. Gửi thư mục embedded signal processing cho firmware.
3. Gửi thư mục PhysioNet/offline cho AI để tham khảo/replay.
4. Gửi file này làm quy ước phạm vi và phiên bản.
5. Không trộn raw, code firmware và PhysioNet vào một thư mục chức năng duy nhất.

## 6. Trạng thái

**Task 2 signal-processing/SQI handoff: READY – v0.1.**

Chưa cần đo hoặc xử lý thêm để bàn giao. Các thay đổi sau này được thực hiện như một phiên bản mới và phải replay lại trên cùng các capture hiện tại.
