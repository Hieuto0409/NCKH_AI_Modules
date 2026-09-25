# Demo Offline NCKH — Hướng dẫn sử dụng

Đề tài: **Hệ thống theo dõi sức khỏe tích hợp (Stress PPG · ECG AF/non-AF · SpO₂)**  
Trạng thái: Kiểm chứng thuật toán offline — **chưa có phần cứng ESP32-S3**.

---

## 1. Chạy demo một lệnh

```bash
python tools/run_offline_demo.py
```

Script sẽ tự động:
1. Biên dịch binary C++ kiểm tra Stress PPG (`g++`)  
2. Đọc dữ liệu Step 2 từ `lib/step2_handoff_sqi_v0_1-…/`  
3. Chạy QC cho cả ba nhánh, trích xuất đặc trưng và gọi model  
4. In bảng tổng hợp trạng thái ra stdout  
5. Lưu kết quả chi tiết vào `tools/output/offline_demo_<timestamp>.json`  

### Yêu cầu

| Công cụ | Phiên bản tối thiểu | Ghi chú |
|---------|---------------------|---------|
| Python  | 3.9+               | `numpy`, `pandas`, `scipy` |
| g++     | C++11               | trong PATH để biên dịch binary Stress / SpO₂ |
| PlatformIO CLI (`pio`) | bất kỳ | chỉ dùng cho bước build firmware |

---

## 2. Ý nghĩa mã trạng thái

| Mã | Biểu tượng | Ý nghĩa |
|----|-----------|---------|
| `RESULT_AVAILABLE` | ✅ | Có kết quả thực từ model hoặc thuật toán đã chạy |
| `TEST_FIXTURE`     | 🔬 | Kết quả từ bộ test/replay tích hợp sẵn của module (không từ Step 2) |
| `QC_REJECTED`      | ⚠️ | Dữ liệu Step 2 không qua kiểm tra chất lượng |
| `NOT_READY`        | ❌ | Thiếu điều kiện kỹ thuật (đơn vị chưa xác minh, phần cứng thiếu…) |
| `SKIPPED`          | ➖ | Công cụ (g++/pio) không có trong PATH |

**Mã thoát**: exit 0 = hoàn thành bình thường (NOT\_READY/TEST\_FIXTURE là giới hạn đã biết).  
exit 1 = lỗi kiểm thử thực sự (diff Stress > 1e-9; binary crash; SpO₂ replay FAIL khi g++ có).

---

## 3. Chi tiết từng nhánh

### 3.1 Stress PPG

- **Nguồn dữ liệu**: `lib/.../step2_handoff_sqi_v0_1/raw/ppg_raw(5).csv` (25 Hz, kênh `slot1_raw`)  
- **QC chấp nhận 60 giây**:  
  - ≥ 80% cửa sổ SQI 5 giây từ `ppg_sqi_windows.csv` đạt `ppg_good_v01 = True`  
  - `fifo_ok = 1` và `fifo_overflow_count = 0` cho tất cả 1500 mẫu  
  - ≥ 40 nhịp phát hiện; `valid_rr_ratio ≥ 0.65`  
- **14 đặc trưng** theo chuẩn `stress_ppg::kFeatureNames` (ms, bpm, %)  
- **Kết quả hai tầng**:  
  - Python (`model.json`) — nhãn "Python"  
  - C++ binary (`stress_ppg::infer`) — nhãn "C++ binary"; sai số so với Python ≤ 1 × 10⁻⁹  
- **Hạn chế**: PPG Step 2 thu ở 25 Hz; model WESAD huấn luyện từ 64 Hz. Đặc trưng HRV (ms) ít bị ảnh hưởng; độ chính xác phát hiện đỉnh PPG có thể thấp hơn.

### 3.2 Nhịp tim PPG (Heart Rate BPM)

- **Nguồn đỉnh**: Tái sử dụng trực tiếp các khoảng PPI hợp lệ (`rr_ms`) từ thuật toán phát hiện đỉnh PPG 60 giây ở Nhánh 1, **không chạy thuật toán phát hiện đỉnh thứ hai**.
- **Công thức tính**:
  $$\text{BPM} = \frac{60000}{\text{median}(\text{PPI}_{\text{ms}})}$$
  - Phương pháp tổng hợp phiên: Sử dụng **trung vị (median)** của các khoảng PPI sinh lý hợp lệ [333.3 ms – 1500.0 ms], đồng bộ với cách tính `bpm_median` của Step 2 (`process_hardware_offline.py`).
  - Ưu điểm: Loại bỏ nhiễu và các nhịp ngoại tâm thu tốt hơn trung bình số học.
- **Diễn giải tham khảo lâm sàng (AHA & NHLBI)**:
  - Khi xác nhận rõ người đo là người lớn đang nghỉ (`is_resting=True`):
    - `< 60 BPM`: *Thấp hơn khoảng tham khảo lúc nghỉ*
    - `60–100 BPM`: *Trong khoảng tham khảo lúc nghỉ*
    - `> 100 BPM`: *Cao hơn khoảng tham khảo lúc nghỉ*
  - Khi chưa đủ bối cảnh (mặc định cho dữ liệu mẫu Step 2, `is_resting=None`):
    - Hiển thị nhãn: `Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ`
    - **Không tự suy diễn trạng thái nghỉ từ SQI hay nhãn Stress**.
- **Kết quả trên dữ liệu mẫu Step 2**:
  - `heart_rate_bpm = 88.24 BPM` (với trung vị PPI = 680.0 ms, 85 khoảng nhịp hợp lệ).
  - Đánh giá: *Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ*.
  - `heart_rate_status = "RESULT_AVAILABLE"`.
- **Quy tắc an toàn (QC)**:
  - Chỉ trả kết quả khi vượt qua toàn bộ QC (SQI ≥ 80%, FIFO đầy đủ, ≥ 40 nhịp hợp lệ).
  - Nếu QC không đạt hoặc thiếu đỉnh: trả `NOT_READY` kèm lý do; **tuyệt đối không xuất `0 BPM`** như nhịp tim đo được.
- **Tuyên bố quan trọng**: Đây là kết quả từ dữ liệu mẫu Step 2, chưa phải phép đo trực tiếp trên phần cứng ESP32-S3.

### 3.3 ECG AF/non-AF

- **Nguồn dữ liệu**: `raw/ecg_raw(8).csv` (500 Hz, file sạch); `ecg_raw(9).csv` (nhiễu, bị từ chối)
- **Cấu hình trích xuất**:
  - Cửa sổ: **30 giây** (15000 mẫu), lọc dải RR trong **[0.2, 2.0] giây**, khớp hoàn toàn với quy trình tạo tập dữ liệu huấn luyện.
  - Quét cửa sổ ứng viên 30s với bước 5s để chọn đoạn tín hiệu sạch ổn định nhất.
- **Model Edge Impulse project 1119067 — đã xác minh từ `model_metadata.h` & `ECG.rar`**:
  - `EI_CLASSIFIER_SENSOR = EI_CLASSIFIER_SENSOR_FUSION`
  - `EI_CLASSIFIER_FUSION_AXES_STRING`:
    `mean_rr + median_rr + sdnn + rmssd + pnn50 + cv_rr + iqr_rr + min_rr + max_rr` (9 đặc trưng)
  - **Đơn vị ĐÃ ĐƯỢC XÁC MINH từ tập huấn luyện gốc (`ECG.rar` bản `_B`)**:
    - 7 đặc trưng thời gian (`mean_rr`, `median_rr`, `sdnn`, `rmssd`, `iqr_rr`, `min_rr`, `max_rr`): **giây (s)**
    - `pnn50`: **%**
    - `cv_rr`: **không thứ nguyên (tỷ lệ)**
    - Các giá trị trung bình trong tập train khớp hoàn toàn với tham số `scaler_mean` trong `model_variables.h`.
- **QC cổng phần cứng**: `clean_fraction ≥ 0.80`, `adc_invalid ≤ 0.005`, `rail ≤ 0.005`, `flat ≤ 0.05`
- **Lưu ý nguồn gốc**: Dữ liệu huấn luyện dùng chú giải `.qrs` ở 250 Hz; demo phát hiện đỉnh R bằng Pan-Tompkins ở 500 Hz.
- **Trạng thái suy luận: `NOT_READY` (lý do kỹ thuật rõ ràng)**:
  - Model Edge Impulse được xuất dưới dạng thư viện MCU/Arduino SDK nhắm đến ESP32-S3 (đã biên dịch thành công trong firmware PlatformIO).
  - Môi trường máy tính x86 thiếu runtime TFLite Micro tương thích nên chưa thể thực thi inference offline trực tiếp.
  - **Giữ nguyên `NOT_READY`, không tạo nhãn giả lập hoặc quy tắc tự viết**.

### 3.4 SpO₂

- **Kết nối Step 2 (`CHƯA ĐỦ ĐIỀU KIỆN`)** — ghi trong JSON riêng `step2_connection`:
  1. Tần số mẫu: Step 2 PPG ở 25 Hz, `Stream100` yêu cầu 100 Hz
  2. Chưa định danh bước sóng: `slot0_raw`/`slot1_raw` chưa xác minh
  3. Không nội suy 25 → 100 Hz; không truyền tín hiệu AC đã lọc vào `pushSpo2()`
- **Module replay test (độc lập)**: `TEST_FIXTURE`
  - Nguồn: `SPO2_Module/tests/data/session_N.csv` (replay 100 Hz)
  - Kết quả: Session 1 = 99%, Session 2 = 100%, Session 3 = 99%
- **Khoảng tham khảo lâm sàng (MedlinePlus & NHS England COVID Oximetry @home)**:
  - `95–100%`: Trong khoảng tham khảo
  - `93–94%`: Cần chú ý
  - `≤ 92%`: Cảnh báo SpO₂ thấp
  - Dữ liệu lỗi / không đạt QC: Chưa có kết quả tin cậy
  - *Áp dụng*: Người lớn lúc nghỉ, gần mực nước biển, không có mục tiêu SpO2 riêng do bác sĩ chỉ định.
- **Hạn chế**: Replay test xác nhận thuật toán đúng — **không phải đo trên người thật hoặc ESP32-S3**.

---

## 4. Kiểm tra bổ sung

```bash
# Kiểm tra các quy tắc hiển thị ECG, SpO2 và PPG BPM
python test/test_interpretation_rules.py

# Kiểm tra độc lập thuật toán tính BPM từ PPG
python test/test_ppg_heart_rate.py

# Cross-check Python vs C++ Stress PPG (6 fixture WESAD)
python test/verify_stress.py
g++ -std=c++11 -I lib/Stress_PPG_60s_model test/test_stress_offline.cpp -o .pio/stress_test.exe
.pio/stress_test.exe

# Cầu nối dữ liệu offline đầy đủ
python tools/offline_data_bridge.py

# Build firmware (không nạp) — chạy từ terminal PlatformIO
pio run -e esp32-s3-devkitc-1
pio run -e test_fixture
```

---

## 5. Những gì còn thiếu để chạy trên phần cứng thật

| Thiếu | Ghi chú |
|-------|---------|
| ESP32-S3 DevKitC-1 | Chưa nạp firmware, chưa có board thực tế |
| MAX30102 thực | Cần xác nhận mapping slot RED/IR qua datasheet và thử nghiệm |
| Tần số mẫu 100 Hz PPG | Cần cấu hình MAX30102 từ 25 Hz → 100 Hz để kết nối SpO₂ |
| Runtime TFLite Micro trên PC x86 | Để chạy EI offline trên desktop; hiện model đã sẵn sàng trong firmware ESP32-S3 |
| Người tình nguyện thực tế | Kiểm chứng lâm sàng nằm ngoài phạm vi đề tài NCKH này |

---

> **Tuyên bố từ chối trách nhiệm**: Toàn bộ kết quả trong tài liệu này và file JSON đầu ra  
> được tạo từ dữ liệu offline và kiểm tra thuật toán nội bộ.  
> **Không có kết quả nào được đo trực tiếp trên cảm biến phần cứng hoặc trên người thật.**
