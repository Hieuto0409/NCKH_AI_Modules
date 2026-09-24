# Mô hình Stress PPG 60 giây có thể tái sử dụng

## Bản nào là bản triển khai?

`model.json`, `stress_hrv_model_deployment.joblib` và `stress_ppg_model.h` là **cùng mô hình triển khai**, fit trên train + validation (266 cửa sổ, 13 người). `stress_hrv_model_evaluation.joblib` chỉ fit train (224 cửa sổ, 11 người); file này dùng để báo cáo kết quả validation 42/42 và test 56/60 (93,33%). Không gộp các phiên bản. Suy luận bằng JSON triển khai trên 60 cửa sổ test: 58/60 (96,67%), một phép kiểm tra bổ sung chứ không phải chỉ số test chính thức của script huấn luyện.

## Nội dung và đầu vào

- `model.json`: scaler và Logistic Regression, mapping 0=baseline/1=stress, ngưỡng 0,5; không yêu cầu runtime Python trên MCU.
- `stress_ppg_model.h`: phép suy luận C++11 từ **đúng 14 đặc trưng**; từ chối số không hữu hạn/sai kích thước. Hàm không kiểm tra chất lượng BVP: ứng dụng phải làm QC trước và từ chối xuất nhãn khi tín hiệu không đạt.
- `predict.py`: lệnh kiểm tra trên CSV đặc trưng hoặc một cửa sổ BVP WESAD 64 Hz; cài `numpy pandas scipy`.
- `tao_dataset_stress_hrv_60s.py`: mã gốc tạo đặc trưng để đối chiếu; phần đọc WESAD dùng pickle, chỉ sử dụng với dữ liệu gốc đáng tin cậy. Đầu vào `--bvp` của `predict.py` **không gọi** chức năng đọc pickle.
- `train_stress_hrv_60s.py` và ba CSV train/validation/test: phục dựng kết quả trong môi trường Python với `scikit-learn matplotlib pandas numpy joblib`. `joblib` cũng là định dạng pickle: chỉ mở bản bạn tin cậy; chạy lại script train từ CSV nếu muốn tạo lại file.
- `stress_hrv_metrics.csv`: metrics khi chạy lại script từ đúng ba CSV.
- `cpp_example.cpp`: ví dụ nạp 14 số đã tính theo thứ tự, không phải hàm phát hiện đỉnh.

Thứ tự đặc trưng: `mean_hr_bpm`, `std_hr_bpm`, `min_hr_bpm`, `max_hr_bpm`, `mean_pp_ms`, `median_pp_ms`, `sdnn_ms`, `rmssd_ms`, `sdsd_ms`, `pnn20_pct`, `pnn50_pct`, `cvnn`, `beat_count`, `valid_rr_ratio`. HR: bpm; PP/SDNN/RMSSD/SDSD: ms; pNN: %; CVNN/valid_rr_ratio: không thứ nguyên. Khoảng PP là khoảng giữa hai đỉnh mạch PPG, không phải RR từ điện tâm đồ.

## Ví dụ

```sh
python predict.py --features stress_hrv_features_60s_test.csv
python predict.py --bvp window_64hz.csv --column BVP
g++ -std=c++11 -O2 cpp_example.cpp -o stress_example
```

Với `--bvp`, file phải chứa đúng 3840 dòng BVP gốc của cửa sổ 60 giây ở 64 Hz. Không nhập CSV PPG 10 giây/125 Hz hoặc mẫu MAX30102 100 Hz trực tiếp vào lệnh này. Nếu tín hiệu không đạt QC, chương trình báo lỗi, không in nhãn stress.

## Ghép vào dự án tổng

Sao chép `stress_ppg_model.h` vào `include/`, gọi `stress_ppg::infer(features, 14)` chỉ sau khi đã trích xuất 14 đặc trưng và tín hiệu đạt QC. Ví dụ:

```cpp
double features[14] = {/* đúng thứ tự như kFeatureNames */};
if (signal_qc_passed) {
    stress_ppg::Result r = stress_ppg::infer(features, 14);
    if (r.valid) { /* hiển thị r.stress và r.stress_probability */ }
}
```

Project `AI IoT System for PPG` được gửi đã nhúng **chính các hệ số này** trong `src/main.cpp` và có hàm `calculateStress()`. Vì vậy việc thay hệ số hoặc chép header vào **không tự làm kết quả chính xác hơn**. Firmware đo IR/MAX30102 ở 100 Hz, phát hiện nhịp bằng thuật toán riêng và sử dụng `rollingIbiValues` tối đa 128 khoảng; pipeline train WESAD lọc BVP 64 Hz bằng Butterworth 0,5–4 Hz, tìm đỉnh trên hai cực tính, tính đặc trưng từ các khoảng PP của cửa sổ 60 giây. Ngay cả khi công thức feature/weight giống nhau, đầu vào có thể khác; không được gọi 93,33% là độ chính xác trên firmware hay MAX30102.

Để triển khai đủ pipeline, thu đủ 60 giây BVP/IR của cảm biến, xây dựng và đối chiếu bộ lọc + phát hiện đỉnh trên phần cứng đích, kiểm tra 14 feature từng cửa sổ so với bản Python dùng **cùng raw signal**, rồi đo và báo cáo kết quả trên người và hoàn cảnh mới. Chưa có trong dữ liệu này kiểm chứng đó. Không dùng kết quả như chẩn đoán y khoa.

## Nguồn và giới hạn

Bộ ba CSV 60 giây có 224/42/60 dòng và 11/2/2 người, test S13 và S16. `stress_hrv_features_60s_all.csv` chỉ có 140 dòng, không khớp ba tập, và báo cáo QC lưu ngưỡng 0,80 khác code hiện tại 0,65. Hai artefact cũ đó không có trong gói tái huấn luyện. WESAD `.pkl` gốc và `subject_split_map.csv` không có trong ZIP, nên chỉ tái lập được từ bảng đặc trưng đã trích xuất. Xem báo cáo Word riêng để biết chi tiết và nguồn tài liệu khoa học.
