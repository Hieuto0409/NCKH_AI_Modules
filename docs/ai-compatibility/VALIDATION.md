# Kiểm thử và điều kiện nghiệm thu

Tài liệu này đi cùng [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md). Baseline là `df46fc1`, không phải trạng thái đã sửa. Các file `evidence/baseline-*` ghi nhận kết quả trên baseline; không ghi đè chúng bằng kết quả mới để làm mất bằng chứng.

## Chạy các probe hiện có

Cần Python 3 standard library, `g++` trong PATH. Runner không tải dữ liệu, không flash bo, không sửa source; nó copy phần source cần thiết vào thư mục tạm rồi compile để tránh lỗi đường dẫn Unicode trên Windows. Nếu thư mục tạm trên máy có Unicode và toolchain không hỗ trợ, dùng môi trường/temp path phù hợp.

Từ repo root:

```powershell
python docs/ai-compatibility/evidence/run_checks.py --output audit-results.json
```

Để đối chiếu 60 vector Stress với repo AI và kiểm tra buffer thư viện MAX30102, cung cấp đường dẫn có sẵn:

```powershell
python docs/ai-compatibility/evidence/run_checks.py --reference ../NCKH_AI_Modules-reference --sparkfun-dir "../CODE/.pio/libdeps/esp32-s3-devkitc-1/SparkFun MAX3010x Pulse and Proximity Sensor Library" --output audit-results.json
```

Các đường dẫn trên là ví dụ cho workspace hiện tại. Ở máy khác dùng checkout AI tại commit pin/HEAD đã được kiểm tra và thư mục SparkFun 1.1.2 thực tế. Không copy dataset hoặc cache `.pio` vào Git. Nếu reference/SparkFun không được cung cấp, runner ghi `SKIPPED` cho phép kiểm tra tương ứng.

**Exit 0 chỉ xác nhận các probe chạy xong và numerical parity đã kiểm tra không lệch; các probe acquisition có thể vẫn in ra lỗi baseline.** Chuyển các trường hợp này thành test có assert trong test suite khi thực hiện sửa. Kết quả số học 60/60 là khớp giữa hai implementation, không phải 60/60 nhãn đúng so với ground truth.

FIFO probe quan sát thư viện SparkFun được truyền vào; sau khi firmware chuyển sang đọc FIFO trực tiếp, probe thư viện vẫn có thể cho kết quả cũ. Test nghiệm thu T01 phải gọi **driver mới của firmware**, không sửa thư viện chỉ để thay kết quả probe. Khi refactor interface/clock, cập nhật harness để gọi interface thật tương ứng, đồng thời giữ nguyên bản ghi baseline và tình huống lỗi cần kiểm tra.

## Ma trận regression bắt buộc

| ID | Phép thử | Tiêu chí sau sửa |
|---|---|---|
| T01 | MAX30102 fake FIFO: batch 1/3/4/8/31, pointer wrap, full 32, random batch boundaries; RED/IR có giá trị phân biệt | Không mất/trùng/đảo thứ tự, đúng channel mapping; full/empty theo datasheet; no zero/stale startup sample; số mẫu và diagnostics đúng. |
| T01-F | I2C short read, NACK, overflow, polling pause, queue full | Không decode partial bytes thành mẫu hợp lệ; continuity break có reason; reset đúng một discontinuity; phần mất không biến thành dữ liệu giả. |
| T02 | Production EcgAcquisition với mock oneshot và clock đứng tại 10.000 µs sau begin=0 | Không tạo sáu mẫu có timestamp 0…10.000 µs từ cùng thời điểm đọc. Chỉ mẫu thực, missing slots được đếm/flag theo policy mới. |
| T02-B | Scheduling 500 Hz với pause 10/20/100 ms và tải OLED/MQTT/logger | Không backfill; đo actual rate/jitter/drops; RR không nối qua gap; backend continuous và battery oneshot không tranh ADC1. Host mock và bench report ghi riêng. |
| T03 | Kết hợp production timestamp reconstructor → SpO2 adapter, batch 4, poll 20.001 µs, 800 sample sequence liên tục | 400 cặp, không reset giả, không NeedData do timestamp jitter. Lặp với jitter ±, batch biến thiên, 799/800/801 samples. |
| T03-F | Sequence skip/duplicate, timestamp lùi, FIFO overflow, invalid continuity, đổi LED/ADC/rate | Drop/reject/reset theo policy, không trộn cặp qua gap; reset không lặp cho cùng event. Unsupported rate phải bị từ chối, không chạy adapter dưới rate sai. |
| T04 | State machine + adapter, mốc start 3.502.900 µs, trạng thái UI millis 3502, poll 63.502.400 µs | Chưa finalize Stress trước đủ 60.000.000 µs; khi đủ cửa sổ và QC tốt thì Ready; cùng start/end được dùng xuyên suốt. |
| T04-B | Start/end ±1 µs, ±1 ms; millis wrap; cancel/restart; duplicate/out-of-order peaks; peak ở đúng biên | Cửa sổ `[start,end)` được áp dụng nhất quán, không double count; không dùng peak trước phiên; full window không bị thiếu chỉ vì UI clock. |
| T04-W | Mẫu warmup và filter state trước Measuring | Warmup nuôi DSP; bắt đầu window xóa thống kê/peak của phiên trước nhưng không làm mất settling đã thực hiện; gap thật reset state phù hợp. |
| T05 | ECG60 có nhịp thay đổi ở giây 30; candidates 30 s bước 5 s | Chỉ tính feature của window đã chọn; provenance start/end đúng. Tie về số valid RR chọn sớm nhất; RR phải có hai peak trong cùng window/segment. |
| T05-Q | Leads-off/dropout/clipping trong/ngoài candidate; không có candidate sạch | Gate dùng chính SQI/integrity của candidate; không dùng counter tích lũy cả phiên; no-clean-window trả lý do rõ, không tự inference. |
| T06 | Cùng peak timestamps/PPI/RR đưa vào C++ và oracle Python nguồn | 14/9 features đúng order, units, beat/interval denominator; std sample; quantile nội suy; boundary PPI/RR, pNN `>` xử lý nhất quán. |
| T06-S | 60 vector Stress nguồn + invalid length/NaN/Inf | Header double: probability abs error ≤1e-9; wrapper float: ≤1e-6; labels trùng; invalid input bị từ chối. Các ngưỡng này là numerical tolerance, không phải clinical tolerance. |
| T06-E | Gọi model ECG thật với feature vectors có provenance, golden probability từ đúng model export | Không chạy nhánh native stub; label order AF/non-AF đúng; xác suất hữu hạn và hợp lệ; tolerance được ghi theo runtime/precision. Không tự đoán xác suất chuẩn. |
| T07 | Raw waveform replay sạch/nhiễu/gap và captures có fs/channel mapping đã biết | Báo peak timing/count, feature/SQI và prediction trước/sau. Không so sánh hai filter khác nhau bằng equality vô căn cứ; giữ test numerical same-peaks riêng khỏi raw DSP validation. |
| T08 | Serial/MQTT/binary encode→decode, schema versions, UI | Xem được cửa sổ/SQI riêng từng nhánh, status/reason/probability/version; consumers khớp schema mới, không đổi nhãn non-AF thành Normal. |

Boundary numerical cần được kiểm tra có chủ đích. Ví dụ chênh RR 50 ms có thể bị float rounding khi đổi sang giây; không tự thay `>` thành `>=` hoặc thêm epsilon mà không đối chiếu oracle và ghi thay đổi policy.

## Oracle và fixture

- Stress model oracle: `model.json` và `stress_hrv_features_60s_test.csv` trong repo AI pin. Runner thực sự chạy header C++ của firmware, không chỉ tính lại Python.
- ECG feature oracle: hàm tính feature của `tools/offline_data_bridge.py` tại commit pin, được cấp **cùng RR và window**, không giả định detector causal giống zero-phase filter.
- ECG inference oracle: cùng exported model/scaler trên runtime chạy thật. Lưu input vector, model hash, runtime, probabilities, nhãn. Nếu chưa có runtime hoặc board, ghi NOT RUN; không coi skip là pass.
- Raw captures: giữ bên ngoài Git khi chưa có quyền/phạm vi publish; fixture synthetic nhỏ có thể commit với nhãn rõ. Không gọi synthetic là dữ liệu bệnh nhân hay kết quả đo thực.
- Không giả định `tin_hieu` reference đang chạy trên production firmware: chứng minh integration bằng build source list và replay.

## Build và bộ test hiện có

Chạy trong `firmware/`:

```powershell
pio test -e native
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1-mqtt
pio run -e esp32-s3-devkitc-1-raw-log
pio run -e esp32-s3-devkitc-1-fixture
```

Windows/đường dẫn Unicode: dùng `tools/build_windows.ps1 -Environment <environment>` cho từng profile. Không flash tự động trong test host. Native vẫn phải pass các test hiện có; thêm acquisition/state-machine/FeatureBuilder vào nguồn native nếu test mới cần, tránh chỉ test adapter tách rời.

Nếu sửa `tin_hieu/`, chạy thêm hai GCC smoke tests trong `tin_hieu/tests/README.md`. Không cần đổi reference chỉ để làm firmware test pass.

## Mức bằng chứng và trạng thái báo cáo

1. **Build:** biên dịch/link được. Baseline default đã SUCCESS, RAM 74.428 byte, flash 400.649 byte.
2. **Numerical:** cùng feature/RR cho cùng kết quả. Baseline Stress 60 vector đã khớp.
3. **Integrated replay:** chạy acquisition/timing/window/gate/model path và regression, không bỏ qua bằng stub. Baseline còn A01–A05.
4. **Board:** serial log thực, timing, FIFO, ADC, model inference/latency, tải UI/MQTT, reset/cancel và soak test. Chưa có bằng chứng trong lần audit này.

Hoàn thành sửa phần mềm khi test/build/replay liên quan đạt và mọi giới hạn còn lại được ghi rõ. Chỉ đánh dấu hardware validation khi có log đo thật; mức này không suy ra độ chính xác lâm sàng. Cập nhật `firmware/CODEX_INTEGRATION_REPORT.md` sau sửa, giữ bản ghi lịch sử và dẫn link kế hoạch để tránh nhầm với trạng thái đã được chứng minh.

## Kết quả sau sửa 0.3.0

Xem [REPAIR_REPORT.md](REPAIR_REPORT.md) và `evidence/repaired-*`.
40 native tests PASS; host ECG export thực chạy 3 vector (native Unity vẫn stub).
T01/T02/T03/T04/T05 có regression và synthetic raw tests. T06 có feature oracle
trích AST nguồn và 60 Stress vectors; T06-E mới là host smoke, chưa golden độc lập.
T07 có synthetic clean/gap/flat/clipped, chưa raw capture thật/annotated peaks.
T08 có production encoder/fake transport round-trip; chưa thử broker/OLED trên bo.
Các biến thể bench, jitter budget, calibration và soak test vẫn NOT RUN.

`run_checks.py --feature-oracle` cần NumPy và `--reference`; không có flag này thì
vẫn dùng standard library. `project_commit` là commit cha nếu chạy khi chưa commit;
`firmware_source_sha256` nhận diện nội dung source thực đang kiểm tra.
