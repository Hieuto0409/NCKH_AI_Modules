# Công thức và quy ước v0.3

Tài liệu này tách phần **logic cố định** khỏi phần chỉ được hiệu chuẩn khi có
MAX30102/AD8232 thật. Các công thức dưới đây là reference engineering, không
phải tiêu chuẩn chẩn đoán y khoa.

## 1. Luồng một mẫu

Với mỗi mẫu đã được đưa về cùng trục thời gian:

1. Đọc `ecg_raw`, `red_raw`, `ir_raw` và `timestamp_us`.
2. Chạy các nhánh biquad causal:
   - ECG morphology: band-pass 0.5--35 Hz.
   - ECG QRS: band-pass 5--20 Hz.
   - Red/IR AC: band-pass 0.5--8 Hz.
   - Red/IR DC: low-pass 0.5 Hz.
3. Lưu mẫu đã lọc vào ring buffer 5 s.
4. Sau mỗi 2.5 s, tính thống kê cửa sổ và quality flags.
5. Đưa ECG morphology/QRS và PPG AC vào peak detector causal nếu cần feature.

`SignalProcessor.c` không biết MAX30102, AD8232, Serial hay Wi-Fi. Driver ở
sketch/firmware phải biến đổi dữ liệu phần cứng thành đúng ba giá trị float và
timestamp trước khi gọi nó.

## 2. Biquad realtime

Mỗi section dùng Direct Form II Transposed:

```text
y[n] = b0*x[n] + z1
z1   = b1*x[n] - a1*y[n] + z2
z2   = b2*x[n] - a2*y[n]
```

Các hệ số được thiết kế lại trong `sp_init(fs_hz)`. Vì vậy không được copy hệ
số đã tính ở 100 Hz sang phần cứng có fs khác.

## 3. Perfusion PPG

Trong một cửa sổ có `N` mẫu AC và DC:

```text
mean_ac = mean(ac)
std_ac  = sqrt(mean((ac - mean_ac)^2))
mean_dc = mean(dc)
perfusion = std_ac / (abs(mean_dc) + 1e-9)
```

PPG được đánh dấu tốt ở mức tham chiếu khi đồng thời:

```text
red_perfusion >= PERFUSION_MIN_RED
ir_perfusion  >= PERFUSION_MIN_IR
```

`0.0005` (Red) và `0.0002` (IR) chỉ là ngưỡng offline của bộ PhysioNet. Khi
dùng MAX30102 thật phải giữ raw log, đo phân bố perfusion và hiệu chuẩn lại.
Không dùng hai ngưỡng này cho WESAD BVP.

## 4. ECG quality mức tham chiếu

```text
ecg_rms_morph = sqrt(mean(ecg_morph^2))
ecg_rms_qrs   = sqrt(mean(ecg_qrs^2))
ecg_good = (ecg_rms_morph >= ECG_RMS_MORPH_MIN)
         && (ecg_rms_qrs   >= ECG_RMS_QRS_MIN)
```

Hai RMS floor trong `config.h` là placeholder theo đơn vị ADC. Chúng phải được
đo lại khi biết range/offset thực tế của AD8232; không được coi là ngưỡng y tế.

## 5. Cửa sổ và gap

- `window_seconds = 5.0`.
- `step_seconds = 2.5`.
- Cửa sổ đầu tiên phát ra sau đủ 5 s; sau đó phát mỗi 2.5 s.
- `timestamp_us` được dùng để phát hiện `dt > 2 * period_expected`.
- Gap chỉ là quality diagnostic; không tự động nội suy hoặc biến gap thành nhịp.

## 6. Peak, RR/PPI và feature

`PeakDetector` là local-maximum detector causal có look-ahead trễ cố định,
refractory và prominence. Nó trả về **index của đỉnh gốc**, không phải index
thời điểm phát hiện. Polarity phải được cấu hình rõ (+1 hoặc -1); không dùng
`abs()` âm thầm.

Với hai đỉnh liên tiếp `p[i]`, `p[i+1]`:

```text
interval_seconds = (p[i+1] - p[i]) / fs_hz
interval_ms      = 1000 * interval_seconds
rate_bpm         = 60000 / median(interval_ms)
```

Chỉ nhận interval trong 0.30--2.00 s ở reference. `SDRR` là sample standard
deviation (`ddof=1`) của các interval hợp lệ. `RMSSD` chỉ lấy hiệu giữa **hai
interval hợp lệ liền kề trong chuỗi gốc**; không bắc cầu qua interval bị loại.

ECG cho RR/HRV; PPG cho PPI/PRV. Đây là feature engineering, chưa phải nhãn
stress hay chẩn đoán rối loạn nhịp.

## 7. Quy tắc quality

Ở reference hiện tại:

```text
ppg_good  = Red perfusion đạt && IR perfusion đạt
ecg_good  = hai ECG RMS floor đạt
both_good = ecg_good && ppg_good
```

Window có quality kém phải được gắn cờ/reject downstream, không cố ép thành
feature hoặc nhãn. `FeatureSet.feature_valid` chỉ bật khi cả hai kênh có đủ số
interval hợp lệ theo cấu hình accumulator.

## 8. Không đưa vào v0.3

- Không có PAT/PTT lâm sàng.
- Không có SpO2 đã hiệu chuẩn.
- Không có EDA/GSR; EDA là phần future work của WESAD, không trộn vào pipeline
  MAX30102/AD8232 này.
- Không có AI model trong C reference.
