# NCKH_AI_Modules

Kho lưu trữ các module AI/Edge AI của đề tài NCKH, bao gồm các mô hình
và thành phần xử lý tín hiệu dùng trong hệ thống IoT hỗ trợ phân tích
tín hiệu sinh lý.

> **Trạng thái hiện tại:** Module **Stress PPG** đã được phục dựng, kiểm
> tra lại và đóng gói ở mức model/inference. Bước tích hợp tiếp theo là
> ghép model với module xử lý tín hiệu PPG chung của nhóm trước khi
> triển khai trên ESP32-S3.

------------------------------------------------------------------------

## 1. Tổng quan hệ thống

Kiến trúc định hướng của hệ thống:

``` text
Sensors
  │
  ├── MAX30102 → Raw PPG
  │
  └── AD8232 / ECG → Raw ECG
           │
           ▼
   Signal Processing / SQI
           │
           ▼
   Feature Extraction
           │
           ├───────────────┐
           │               │
           ▼               ▼
     Stress Model     Arrhythmia Model
           │
           └───────────────┐
                           ▼
                     SpO₂ / other
```

Đối với **Stress PPG**, pipeline được xác định như sau:

``` text
MAX30102 / PPG
      ↓
Raw PPG
      ↓
Module xử lý tín hiệu của nhóm
      ↓
Filtering / QC / Peak Detection
      ↓
PP / IBI
      ↓
14 HR/PRV features
      ↓
StandardScaler
      ↓
Logistic Regression
      ↓
Stress probability
      ↓
Threshold = 0.5
      ↓
Baseline / Stress
```

### Phân chia trách nhiệm

**Module xử lý tín hiệu của nhóm trưởng** - Thu nhận Raw PPG. - Lọc tín
hiệu. - Kiểm tra chất lượng tín hiệu. - Phát hiện pulse peak. - Tính
PP/IBI. - Tạo các đặc trưng cần thiết.

**Module Stress PPG** - Nhận vector 14 feature. - Chuẩn hóa bằng
StandardScaler. - Chạy Logistic Regression. - Tính xác suất Stress. -
Phân loại Baseline/Stress.

> `stress_ppg_model.h` **không nhận raw PPG trực tiếp**. Nó nhận 14 đặc
> trưng đã được trích xuất.

------------------------------------------------------------------------

# 2. Module Stress PPG

## 2.1. Mục tiêu

Xây dựng mô hình Machine Learning phân loại hai trạng thái:

-   `Baseline`
-   `Stress`

dựa trên tín hiệu BVP/PPG.

Mô hình được xây dựng và đánh giá trên bộ dữ liệu **WESAD**.

Đây là mô hình nghiên cứu phân loại trạng thái trong dataset, **không
phải công cụ chẩn đoán y khoa**.

------------------------------------------------------------------------

# 3. Dataset WESAD

Nguồn dữ liệu:

**WESAD -- Wearable Stress and Affect Detection**

Tín hiệu sử dụng trong pipeline Stress PPG:

-   Wrist BVP/PPG
-   Sampling rate: **64 Hz**
-   Nguồn: Empatica E4 trong WESAD
-   Nhãn sử dụng:
    -   Baseline
    -   Stress

Pipeline HR/PRV chính sử dụng:

-   Window: **60 giây**
-   Step: **30 giây**

### Subject-wise split

  ------------------------------------------------------------------------
  Tập          Subjects          Số cửa sổ        Baseline          Stress
  ------------ ----------- --------------- --------------- ---------------
  Train        S2, S3, S4,             224             112             112
               S5, S6, S7,                                 
               S9, S10,                                    
               S11, S15,                                   
               S17                                         

  Validation   S8, S14                  42              21              21

  Test         S13, S16                 60              30              30
  ------------------------------------------------------------------------

Tổng cộng pipeline hiện tại có:

**326 cửa sổ**

Cách chia được thực hiện theo **subject**, nhằm tránh cửa sổ của cùng
một người xuất hiện ở nhiều tập.

Đây là một lần split theo người, không phải leave-one-subject-out và
không phải đánh giá trên một dataset hoàn toàn độc lập.

------------------------------------------------------------------------

# 4. Phân biệt nhánh dữ liệu 10 giây và pipeline 60 giây

Trong artefact cũ có các script/dataset liên quan đến PPG 10 giây:

``` text
Stress_WESAD_PPG.rar
Baseline_WESAD_PPG.rar
tao_du_lieu_stress_wesad.py
tao_du_lieu_stress_wesad(1).py
chia_baseline_stress_theo_subject.py
```

Nhánh này có xử lý:

``` text
64 Hz → 125 Hz
```

Tuy nhiên **model Stress HRV chính thức** sử dụng:

``` text
tao_dataset_stress_hrv_60s.py
```

với:

``` text
Window = 60 s
Step = 30 s
Sampling rate = 64 Hz
Không resample trong pipeline HRV 60 s
```

Không được trộn nhánh 10 giây/125 Hz với pipeline 60 giây nếu chưa kiểm
tra tương thích.

------------------------------------------------------------------------

# 5. Preprocessing Stress PPG

Pipeline hiện tại:

``` text
WESAD BVP 64 Hz
      ↓
Window 60 s
      ↓
Quality Control
      ↓
Nội suy khoảng lỗi nhỏ nếu hợp lệ
      ↓
Butterworth Bandpass
0.5 – 4 Hz
      ↓
Peak Detection
      ↓
PP/IBI Cleaning
      ↓
14 HR/PRV Features
```

Các thông số đã được xác minh từ code hiện tại:

-   Window: 60 giây = 3840 samples ở 64 Hz.
-   Step: 30 giây.
-   Butterworth bậc 4.
-   Bandpass: 0.5--4 Hz.
-   `sosfiltfilt` được sử dụng.
-   Peak detection dùng `find_peaks`.
-   Xem xét cả cực tính dương và âm.
-   Khoảng cách tối thiểu giữa peak khoảng 1/3 giây.
-   Có kiểm tra prominence.
-   PP/IBI được lọc theo giới hạn sinh lý và theo trung vị.
-   Một cửa sổ cần tối thiểu số peak hợp lệ theo code.
-   `valid_rr_ratio` tối thiểu theo code là 0.65.

Các chi tiết triển khai chính thức phải được lấy từ source code khi tích
hợp lại với module xử lý tín hiệu của nhóm.

------------------------------------------------------------------------

# 6. PP/IBI và PRV

Trong code, một số biến có tên `rr`, nhưng với tín hiệu PPG chúng biểu
diễn khoảng giữa các pulse peak.

Do đó về mặt thuật ngữ nên hiểu:

``` text
PP / IBI
```

và biến thiên của các khoảng PP được gọi là:

``` text
PRV – Pulse Rate Variability
```

Không nên gọi đây là HRV đo trực tiếp từ ECG.

Các chỉ số như SDNN, RMSSD, SDSD... được tính trên các khoảng PP/IBI của
PPG.

------------------------------------------------------------------------

# 7. 14 đặc trưng của Stress PPG

Model sử dụng đúng **14 feature**, theo đúng thứ tự:

``` text
1.  mean_hr_bpm
2.  std_hr_bpm
3.  min_hr_bpm
4.  max_hr_bpm
5.  mean_pp_ms
6.  median_pp_ms
7.  sdnn_ms
8.  rmssd_ms
9.  sdsd_ms
10. pnn20_pct
11. pnn50_pct
12. cvnn
13. beat_count
14. valid_rr_ratio
```

  Feature            Ý nghĩa                                         Đơn vị
  ------------------ ----------------------------------------------- ------------------
  `mean_hr_bpm`      HR trung bình                                   bpm
  `std_hr_bpm`       Độ lệch chuẩn HR                                bpm
  `min_hr_bpm`       HR nhỏ nhất                                     bpm
  `max_hr_bpm`       HR lớn nhất                                     bpm
  `mean_pp_ms`       PP trung bình                                   ms
  `median_pp_ms`     PP trung vị                                     ms
  `sdnn_ms`          Độ lệch chuẩn PP                                ms
  `rmssd_ms`         Căn trung bình bình phương sai phân liên tiếp   ms
  `sdsd_ms`          Độ lệch chuẩn sai phân liên tiếp                ms
  `pnn20_pct`        Tỷ lệ `|Δ| > 20 ms`                             \%
  `pnn50_pct`        Tỷ lệ `|Δ| > 50 ms`                             \%
  `cvnn`             SDNN / mean PP                                  Không thứ nguyên
  `beat_count`       Số pulse peak được phát hiện                    đỉnh
  `valid_rr_ratio`   PP sạch / PP thô                                tỷ lệ

Công thức chính:

``` text
HR_i = 60000 / I_i

SDNN = std(I)

RMSSD = sqrt(mean(Δ²))

SDSD = std(Δ)

pNN20 = 100 × mean(|Δ| > 20 ms)

pNN50 = 100 × mean(|Δ| > 50 ms)

CVNN = SDNN / mean(I)

valid_rr_ratio =
    số PP sau làm sạch / số PP thô
```

Trong code, độ lệch chuẩn sử dụng sample standard deviation (`ddof=1`).

------------------------------------------------------------------------

# 8. Machine Learning Model

Pipeline:

``` text
14 HR/PRV features
        ↓
StandardScaler
        ↓
Logistic Regression
        ↓
Probability
        ↓
Threshold = 0.5
        ↓
Baseline / Stress
```

Cấu hình:

``` text
Classifier: Logistic Regression
C: 0.1
max_iter: 5000
random_state: 42
Threshold: 0.5
```

Chuẩn hóa:

``` text
z = (x - μ_train) / scale_train
```

Trong đó `μ_train` và `scale_train` được học từ training set.

Validation và Test chỉ sử dụng scaler đã fit trên training.

------------------------------------------------------------------------

# 9. Evaluation Model và Deployment Model

Có hai khái niệm cần phân biệt.

## 9.1. Evaluation model

Mục đích:

Đánh giá hiệu năng chính thức.

``` text
TRAIN
  ↓
fit StandardScaler
  ↓
fit Logistic Regression
  ↓
Validation
  ↓
Test
```

Evaluation model chỉ fit trên Training.

## 9.2. Deployment model

Sau khi hoàn thành đánh giá:

``` text
TRAIN + VALIDATION
        ↓
fit model
        ↓
export parameters
        ↓
deployment
```

Các tham số deployment được lưu trong:

``` text
stress_hrv_deployment_parameters.json
```

Bao gồm:

-   `class_0 = baseline`
-   `class_1 = stress`
-   threshold
-   scaler mean
-   scaler scale
-   Logistic Regression coefficients
-   intercept
-   feature order

------------------------------------------------------------------------

# 10. Kết quả đánh giá chính thức

## Validation

``` text
42 / 42 đúng
Accuracy = 100%
```

Confusion matrix:

``` text
[[21, 0],
 [ 0,21]]
```

Các metric:

  Metric                Validation
  ------------------- ------------
  Accuracy                    100%
  Precision Stress            100%
  Recall Stress               100%
  F1 Stress                   100%
  Specificity                 100%
  Balanced Accuracy           100%
  ROC-AUC                    1.000

## Test

``` text
56 / 60 đúng
Accuracy = 93.33%
```

Confusion matrix:

``` text
                Predicted
              Baseline Stress
Actual Baseline    26      4
Actual Stress       0     30
```

  Metric                  Test
  ------------------- --------
  Accuracy              93.33%
  Precision Stress      88.24%
  Recall Stress           100%
  F1 Stress             93.75%
  Specificity           86.67%
  Balanced Accuracy     93.33%
  ROC-AUC                1.000

Diễn giải:

-   4 Baseline bị dự đoán nhầm thành Stress.
-   0 Stress bị bỏ sót trong tập test hiện tại.
-   Test gồm hai subject: S13 và S16.

Không được diễn giải 93.33% như độ chính xác trên 60 người.

------------------------------------------------------------------------

# 11. 93.33% và 96.67%

Hai kết quả này phải được giữ riêng.

## 93.33%

Đây là **test accuracy chính thức** của `evaluation_model`.

``` text
evaluation_model
→ fit TRAIN
→ Test
→ 56/60 = 93.33%
```

Đây là con số dùng để báo cáo hiệu năng chính thức.

## 96.67%

Sau khi đánh giá, `deployment_model` được fit trên:

``` text
TRAIN + VALIDATION
```

Sau đó tham số deployment được áp riêng lên 60 test samples:

``` text
58/60 = 96.67%
```

Đây là **deployment verification / hậu kiểm**, không thay thế test
accuracy chính thức.

Không chọn 96.67% chỉ vì cao hơn.

------------------------------------------------------------------------

# 12. Portable Module

Module portable:

``` text
Stress_PPG_60s_model_portable.zip
```

Các thành phần quan trọng gồm:

``` text
model.json
stress_ppg_model.h
stress_hrv_model_deployment.joblib
stress_hrv_model_evaluation.joblib
predict.py
cpp_example.cpp
train_stress_hrv_60s.py
tao_dataset_stress_hrv_60s.py
stress_hrv_features_60s_train.csv
stress_hrv_features_60s_validation.csv
stress_hrv_features_60s_test.csv
stress_hrv_metrics.csv
README.md
```

### `stress_ppg_model.h`

Đây là module inference C++.

Input:

``` text
14 features
```

Output:

``` text
Baseline / Stress
```

Pipeline:

``` text
14 features
   ↓
StandardScaler
   ↓
Logistic Regression
   ↓
Probability
   ↓
Threshold 0.5
   ↓
Baseline / Stress
```

Header này **không xử lý raw PPG**.

------------------------------------------------------------------------

# 13. Định hướng tích hợp với module Signal Processing của nhóm

Đây là hướng triển khai chính thức:

``` text
MAX30102
   ↓
Raw PPG
   ↓
Signal Processing module
   ↓
Filtering
   ↓
Signal Quality / QC
   ↓
Peak Detection
   ↓
PP / IBI
   ↓
14 HR/PRV Features
   ↓
stress_ppg_model.h
   ↓
StandardScaler
   ↓
Logistic Regression
   ↓
Stress probability
   ↓
Threshold 0.5
   ↓
Baseline / Stress
```

### Phân công module

**Signal Processing module** - Raw PPG - Filtering - QC/SQI - Peak
detection - PP/IBI - Feature extraction

**Stress PPG AI module** - Nhận 14 features - Scaling - Logistic
Regression - Probability - Classification

Không cần tạo thêm một bộ filter riêng cho Stress PPG nếu module signal
processing chung của nhóm đã đảm nhiệm phần đó.

------------------------------------------------------------------------

# 14. Điều kiện để hai module tương thích

Trước khi ghép vào ESP32-S3, cần kiểm tra:

-   Sampling rate
-   Window length
-   Window step
-   Filter
-   Filter frequency
-   Peak detection
-   Peak polarity
-   PP/IBI unit
-   Milliseconds conversion
-   Outlier rejection
-   `valid_rr_ratio`
-   Minimum beat count
-   14 feature definition
-   Feature order
-   StandardScaler
-   Logistic Regression parameters
-   Threshold

Đặc biệt cần đảm bảo **14 feature được tính theo đúng định nghĩa và đúng
thứ tự của model đã train**.

Nếu module signal processing tạo feature khác định nghĩa model cũ, không
nên chỉ nối trực tiếp.

Cần tạo một lớp adapter hoặc điều chỉnh feature extraction để tương
thích.

------------------------------------------------------------------------

# 15. ESP32-S3 Deployment

Mục tiêu cuối:

``` text
MAX30102
    ↓
ESP32-S3
    ↓
PPG acquisition
    ↓
Signal Processing
    ↓
14 features
    ↓
stress_ppg_model.h
    ↓
Baseline / Stress
```

Hiện tại chưa được tuyên bố:

-   realtime
-   thời gian inference
-   RAM usage
-   CPU usage
-   numerical equivalence
-   hiệu năng trên MAX30102

cho tới khi benchmark thực tế trên phần cứng.

------------------------------------------------------------------------

# 16. WESAD và MAX30102

Một điểm bắt buộc phải ghi rõ:

Model được train trên:

``` text
WESAD wrist BVP / Empatica E4
```

không phải dữ liệu MAX30102.

Do đó:

``` text
93.33%
```

là kết quả trên WESAD.

Không được viết:

> Model đạt 93.33% trên MAX30102.

Muốn xác nhận hiệu năng trên MAX30102 cần có:

``` text
MAX30102 data
    ↓
Signal Processing
    ↓
14 features
    ↓
Stress Model
    ↓
Evaluation
```

------------------------------------------------------------------------

# 17. Hạn chế

Các hạn chế hiện tại:

1.  WESAD chỉ có 15 subjects.
2.  Test chỉ có 2 subjects.
3.  Các cửa sổ có thể chồng lấn.
4.  Không thể xem 60 cửa sổ là 60 người độc lập.
5.  Đây không phải leave-one-subject-out.
6.  Chưa có external dataset validation.
7.  BVP của Empatica E4 khác tín hiệu từ MAX30102.
8.  Chưa có bằng chứng end-to-end trên MAX30102.
9.  Chưa có clinical validation.
10. Nhãn Baseline/Stress của WESAD là nhãn trạng thái theo protocol
    nghiên cứu, không phải chẩn đoán lâm sàng.

------------------------------------------------------------------------

# 18. Các artefact đã xác minh

### Pipeline chính

``` text
tao_dataset_stress_hrv_60s.py
train_stress_hrv_60s.py
```

### Dataset feature

``` text
stress_hrv_features_60s_train.csv
stress_hrv_features_60s_validation.csv
stress_hrv_features_60s_test.csv
```

### Deployment

``` text
stress_hrv_deployment_parameters.json
stress_hrv_model_deployment.joblib
stress_ppg_model.h
model.json
```

### Inference

``` text
predict.py
cpp_example.cpp
```

### Artefact liên quan

``` text
tao_du_lieu_stress_wesad.py
tao_du_lieu_stress_wesad(1).py
chia_baseline_stress_theo_subject.py
Stress_WESAD_PPG.rar
Baseline_WESAD_PPG.rar
```

------------------------------------------------------------------------

# 19. Những điểm chưa xác minh

Hiện tại chưa thể xác minh đầy đủ:

-   Tái dựng từ 15 file WESAD `.pkl` gốc vì các file gốc không nằm trong
    ZIP.
-   Nguyên nhân chính xác của một số artefact khác như
    `stress_hrv_features_60s_all.csv`.
-   Nguyên nhân chính xác của các khác biệt trong
    `stress_hrv_qc_report.csv`.
-   Accuracy trên MAX30102.
-   Hiệu năng trên dữ liệu thực địa.
-   Benchmark CPU/RAM/thời gian chạy trên ESP32-S3.
-   Numerical equivalence giữa Python và MCU sau khi tích hợp toàn bộ
    signal processing.

------------------------------------------------------------------------

# 20. Checklist triển khai

## Dataset / Model

-   [x] WESAD
-   [x] BVP/PPG 64 Hz
-   [x] 60 s window
-   [x] 30 s step
-   [x] 14 features
-   [x] Subject-wise split
-   [x] Logistic Regression
-   [x] StandardScaler
-   [x] Threshold 0.5
-   [x] Validation 100%
-   [x] Official Test 93.33%
-   [x] Deployment parameters
-   [x] C++ inference module

## Integration

-   [ ] Nhận module signal processing của nhóm trưởng
-   [ ] Kiểm tra sampling rate
-   [ ] Kiểm tra filter
-   [ ] Kiểm tra peak detection
-   [ ] Kiểm tra PP/IBI
-   [ ] Kiểm tra 14 feature
-   [ ] Kiểm tra feature order
-   [ ] Offline integration test
-   [ ] ESP32-S3 integration
-   [ ] RAM/CPU benchmark
-   [ ] Realtime test
-   [ ] MAX30102 validation

------------------------------------------------------------------------

# 21. Kết luận

Stress PPG model hiện tại đã được phục dựng và kiểm tra lại từ các
artefact của project.

Pipeline chính:

``` text
WESAD wrist BVP 64 Hz
        ↓
60 s / 30 s window
        ↓
Signal preprocessing
        ↓
PP / IBI
        ↓
14 HR/PRV features
        ↓
StandardScaler
        ↓
Logistic Regression
        ↓
Threshold 0.5
        ↓
Baseline / Stress
```

Kết quả chính thức:

``` text
Validation:
42/42 = 100%

Test:
56/60 = 93.33%
```

Module portable đã chứa model và các tham số cần thiết cho inference.

Bước tiếp theo không phải train lại model, mà là **tích hợp model với
module xử lý tín hiệu PPG chung của nhóm**, sau đó kiểm thử trên
ESP32-S3 và cuối cùng đánh giá bằng dữ liệu thực tế từ MAX30102.

------------------------------------------------------------------------

# 22. Tài liệu tham khảo

1.  Schmidt P, Reiss A, Duerichen R, Marberger C, Van Laerhoven K.
    *Introducing WESAD, a Multimodal Dataset for Wearable Stress and
    Affect Detection*. ICMI 2018. DOI: 10.1145/3242969.3242985.

2.  UCI Machine Learning Repository. *WESAD: Wearable Stress and Affect
    Detection*. DOI: 10.24432/C57K5T.

3.  Task Force of the ESC and NASPE. *Heart rate variability: standards
    of measurement, physiological interpretation, and clinical use*.
    Circulation. 1996;93:1043--1065.

4.  Allen J. *Photoplethysmography and its application in clinical
    physiological measurement*. Physiological Measurement.
    2007;28:R1--R39. DOI: 10.1088/0967-3334/28/3/R01.

5.  Schäfer A, Vagedes J. *How accurate is pulse rate variability as an
    estimate of heart rate variability? A review of current studies*.
    International Journal of Cardiology. 2013.

6.  scikit-learn. *StandardScaler documentation*.

7.  scikit-learn. *LogisticRegression documentation*.

------------------------------------------------------------------------

## Version

``` text
Stress PPG module
Pipeline: 60 s HR/PRV
Dataset: WESAD
Classifier: Logistic Regression
Official Test Accuracy: 93.33%
Deployment verification: 96.67%
Target hardware: ESP32-S3
Sensor target: MAX30102
Status: AI model completed; system integration pending
```
