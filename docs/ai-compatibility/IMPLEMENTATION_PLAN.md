# Kế hoạch sửa tương thích ECG/PPG và AI cho Codex

> Đã triển khai firmware 0.3.0 theo kế hoạch này. Nội dung mô tả lỗi bên dưới là
> **baseline lịch sử**; đọc [REPAIR_REPORT.md](REPAIR_REPORT.md) trước khi sửa tiếp.
> Kiểm chứng trên bo và golden ECG độc lập vẫn còn mở.

Ngày lập kế hoạch: 2026-09-26. **Baseline khi lập: chưa sửa hành vi firmware.**
Trạng thái hiện hành: đã triển khai 0.3.0; xem báo cáo sửa và checklist cuối tài liệu.

## 1. Mục tiêu và phạm vi

Sửa luồng **thu mẫu → timestamp/integrity → DSP/peak → cửa sổ/SQI → feature → model → kết quả** trong `firmware/` để dùng đúng hai model hiện có. Giữ tách `firmware/` và `tin_hieu/` trong cùng repo dự án. Không cần huấn luyện lại hoặc đổi model để xử lý các lỗi đã xác nhận bên dưới.

Baseline dự án: [`df46fc1`](https://github.com/Hieuto0409/EdgeAI-PPG-Screening/tree/df46fc1754390f89cc49a912a391db4b08a9b7cb).
AI pin: [`7d46835`](https://github.com/Hieuto0409/NCKH_AI_Modules/tree/7d4683581c8c01ff6878bcad31a601aff271f2a0).
HEAD AI đã kiểm tra: [`6f584d6`](https://github.com/Hieuto0409/NCKH_AI_Modules/tree/6f584d6632f08e77a893337f93b2c63ba13fcb1a).
Từ pin đến HEAD AI chỉ đổi tài liệu. Nội dung các file model đã vendoring khớp sau chuẩn hóa LF/CRLF: Stress 2 file, ResearchSpO2 5 file, ECG/SDK 1.383 file. Không coi metadata SDK 1 ms là độ dài cửa sổ ECG; đó là metadata đầu vào feature.

Trước khi sửa, fetch/kiểm tra commit hiện hành và diff với baseline, đọc `AGENTS.md`, bảo toàn thay đổi của người khác. Không coi báo cáo tích hợp cũ là bằng chứng các lỗi sau đã được sửa.

## 2. Hợp đồng phải giữ

| Nhánh | Hợp đồng |
|---|---|
| Stress | PPG 60 s; tối thiểu 40 peak; PPI trong [60000/180, 1500] ms; median của PPI qua giới hạn sinh lý; giữ ±30%; tỷ lệ hợp lệ ≥0,65 trên **tổng PPI thô**. Mean/std/min/max HR, mean/median PPI, SDNN, RMSSD, SDSD, pNN20, pNN50, CVNN, **số peak**, valid ratio, đúng thứ tự model. Std mẫu `ddof=1`; pNN dùng %, ngưỡng `>`; các đặc trưng interval dùng ms. Header tự chuẩn hóa một lần. |
| ECG AF/non-AF | 30 s ECG; RR hợp lệ [0,2; 2,0] s; tối thiểu 3 RR theo pipeline nguồn. Thứ tự `mean_rr, median_rr, sdnn, rmssd, pnn50, cv_rr, iqr_rr, min_rr, max_rr`. Thời gian dùng s, pNN50 dùng %, CV không đơn vị; SDNN mẫu, percentile nội suy tuyến tính. Edge Impulse tự chuẩn hóa; không chuẩn hóa thêm trong adapter. |
| SpO2 | Thu RED/IR thô đồng bộ 200 Hz, còn DC; trung bình từng cặp liên tiếp → 100 Hz; 400 cặp sau giảm mẫu cho 4 s. Không nội suy dữ liệu mất thành mẫu hợp lệ. Đây là thuật toán, không phải model AI. |
| Sampling/phần cứng | MAX30102 200 Hz, average 1, RED+IR, pulse 411 µs, ADC range khởi tạo 8192, LED khởi tạo 0x3F; ADC1 ECG mục tiêu 500 Hz. Không đổi tần số toàn hệ thống để né adapter. |
| GPIO | MAX30102 SDA/SCL 12/13, INT không nối; ECG OUT/LO+/LO− 1/5/6; OLED CS/DC/RESET/MOSI/SCLK 42/41/40/39/38; BTN1/2 48/47; battery ADC 8. Không đổi pin để sửa tương thích. |

Nguồn tính feature: [Stress training extractor](https://github.com/Hieuto0409/NCKH_AI_Modules/blob/7d4683581c8c01ff6878bcad31a601aff271f2a0/lib/Stress_PPG_60s_model/tao_dataset_stress_hrv_60s.py), [ECG offline bridge](https://github.com/Hieuto0409/NCKH_AI_Modules/blob/7d4683581c8c01ff6878bcad31a601aff271f2a0/tools/offline_data_bridge.py), model headers/metadata đã vendoring. Các giới hạn GPIO/sampling cũng được ghi trong `firmware/include/config/` và báo cáo tích hợp hiện có.

## 3. Lỗi đã xác nhận và cách sửa

### A01 — P1: đường đọc MAX30102 có thể mất mẫu ở buffer thư viện

**Bằng chứng:** `firmware/src/drivers/max30102_driver.cpp::drain()` gọi `sensor_.check()` rồi mới lấy `available()`. SparkFun 1.1.2 đang cài có `STORAGE_SIZE=4`; `check()` đọc cả batch vào ring này, `available()` tính head−tail modulo 4. Probe chạy nguyên thân hàm thư viện với I2C giả: 4 hoặc 8 mẫu phần cứng → available 0; 31 mẫu → available 3. Probe này xác nhận logic buffer, chưa phải đo I2C trên bo. Buffer ngoài `PpgFifoSample batch[32]` không khắc phục ring nội bộ.

**Hướng triển khai chọn:** giữ thư viện để cấu hình cảm biến nhưng thay đường drain bằng đọc trực tiếp FIFO theo chunk đầy đủ RED/IR 6 byte, có đếm số mẫu và kiểm tra số byte thực nhận. Không gọi `sensor_.check()` trên đường này; không trộn hai consumer đọc cùng FIFO. Đối chiếu datasheet MAX30102 cho pointer wrap, full/empty, overflow counter/reset, thứ tự RED/IR trước khi triển khai. Nếu chọn vendoring/fix thư viện thay thế, phải chứng minh đủ dung lượng, FIFO order và empty/full khác nhau; giữ attribution, pin phiên bản. Không sửa cache `.pio/libdeps` làm giải pháp lâu dài.

**File:** driver MAX30102, acquisition PPG, types/diagnostics và dependency config. Pin chính xác phiên bản dependency được kiểm chứng, không chỉ dựa vào caret range.

**Nghiệm thu:** mọi mẫu 1/3/4/8/31 trong fake FIFO ra đúng giá trị, thứ tự và số lượng; trường hợp full 32 phải được xử lý theo datasheet; chunk/short-read/NACK/wrap/overflow tạo lỗi hoặc flags đúng, không biến thành mẫu zero hợp lệ. Xem T01.

### A02 — P1: ECG oneshot đang tạo lịch sử mẫu giả khi vòng lặp trễ

**Bằng chứng:** `ecg_acquisition.cpp::poll()` đọc bù tối đa 8 lần, gán `next_sample_us_` quá khứ cho kết quả `analogRead()` hiện tại. Probe production với backend/clock giả: một lượt poll tại 10.000 µs đọc ADC 6 lần, gán 0/2000/4000/6000/8000/10000 µs, received=6, dropped=0. Backend oneshot không có FIFO lịch sử; vì vậy không thể coi đó là sáu mẫu 500 Hz độc lập đúng thời gian.

**Hướng triển khai:** bỏ catch-up bằng đọc bù oneshot. Ưu tiên ADC1 continuous/DMA nếu core Arduino/IDF đã chọn hỗ trợ đúng; nếu không, dùng timer/scheduler thu thực với timestamp và bounded queue, theo hạn chế thread/ISR của API ADC. Không gọi API blocking từ ISR nếu API không cho phép. Tách việc thu khỏi OLED/MQTT/logger. Nếu fallback bị lỡ lịch, đánh dấu missed slots/dropout, chỉ đọc mẫu thực hiện tại; không backdate mẫu mới để che mất mẫu.

Giữ backend interface và pin ADC1; nếu dùng continuous, không mở ADC1 oneshot đồng thời cho battery. Đo battery trong chiến lược continuous/decimation hoặc khi driver dừng. Log actual sample count, lateness, drops và rate. Mục tiêu 500 Hz cần đo trên bo; việc cấu hình con số 500 chưa đủ.

**File:** ECG backend/acquisition, app scheduling, queue/integrity, platform config nếu cần.

**Nghiệm thu:** pause scheduler 10/20/100 ms không tạo mẫu lùi thời gian; samples missing được đánh dấu; các RR đi qua gap không được đưa vào feature như liên tục. Xem T02.

### A03 — P1: timestamp PPG và bộ SpO2 không cùng quy ước liên tục

**Bằng chứng:** reconstructor giữ phần lệch poll dưới chu kỳ; adapter lại reset nếu delta khác đúng 5000 µs. Probe 800 mẫu, batch 4, poll period 20.001 µs: không inferred gap/sequence gap/overflow, nhưng 199 reset, chỉ giữ 2 cặp, `NeedData`. Poll period 20.000 µs: 400 cặp, 0 reset.

**Hướng triển khai chọn:** sau khi sửa A01, timestamp logical của PPG được tạo từ sample counter và mốc 64-bit ổn định; khi đã chứng minh FIFO liên tục thì timestamp kế tiếp là timestamp trước +5000 µs, không neo lại từng poll. Thời gian poll/độ trễ/clock drift là diagnostics riêng. Đừng xem số mẫu thực nhận sau buffer lỗi là bằng chứng không mất mẫu. Overflow, lỗi FIFO/short-read, queue drop hoặc bất định continuity phải tạo một discontinuity event và reset adapter/segment một lần.

Không chỉ xóa điều kiện reset hoặc tăng dung sai vô hạn: giữ phát hiện non-monotonic/duplicate/mất mẫu thực; ghi rõ tiêu chí reconnect, long stall và re-anchor khi continuity không còn xác định. Sai số clock dài hạn cần log/bench validation, không sửa sample rate bằng cách lặng lẽ co giãn từng timestamp.

**File:** `timestamp_service.*`, `ppg_acquisition.*`, `spo2_rate_adapter.*`, sample flags/integrity.

**Nghiệm thu:** jitter poll dương/âm và batch thay đổi không reset khi dữ liệu liên tục; gap thật/config change vẫn reset; đúng 800 source samples →400 pairs. Xem T03.

### A04 — P2: phiên đo và Stress dùng hai mốc thời gian khác độ phân giải

**Bằng chứng:** state machine hoàn tất theo millis, adapter yêu cầu đủ 60.000.000 µs từ một timestamp lấy sau đó. Probe: start millis=3502/start_us=3502900, finish millis=63502/finish_us=63502400; máy đã hoàn tất nhưng Stress mới có 59.999.500 µs và trả NotReady. Thêm 500 µs thì cùng dữ liệu cho Ready.

**Hướng triển khai chọn:** một `measurement_start_us` và một deadline `start+60'000'000` do monotonic clock sở hữu. State machine và feature window dùng cùng hai giá trị; UI millis chỉ để trình bày. Đánh giá một lần khi đã drain các mẫu nằm trong cửa sổ. Chọn biên cửa sổ `[start,end)` đồng nhất với slicing N mẫu trong reference, không vô tình lấy mẫu/peak ngoài phiên; quản lý lookahead/latency detector rõ ràng nếu peak cuối cửa sổ cần mẫu sau deadline. Không giảm yêu cầu 60 s hay chỉ cộng delay tùy ý.

Tách `resetPipeline()` lúc bắt đầu/đứt tín hiệu và `beginMeasurementWindow()` xóa thống kê/counters. Hiện Warmup không đưa mẫu vào DSP và `FeatureBuilder::reset()` lại reset filter khi Measuring bắt đầu; warmup vì thế chưa làm filter ổn định. Cho warmup nuôi filter/detector, rồi bắt đầu cửa sổ không mang peak/interval trước start vào feature. Nếu chủ đích có settling segment, đặt nó trước start cửa sổ đủ 60 s.

**File:** state machine/controller, FeatureBuilder lifecycle, Stress adapter bounds.

**Nghiệm thu:** boundary ±1 µs/±1 ms, rollover millis, cancel/restart và detector latency đều cho cửa sổ đúng; không suy luận Stress trước deadline và không NotReady chỉ do lệch đơn vị clock. Xem T04.

### A05 — P2: ECG cần cửa sổ 30 s cùng SQI, không dùng RR cả phiên 60 s

**Bằng chứng:** FeatureBuilder truyền toàn bộ RR thu từ phiên 60 s; adapter không nhận timestamp nên không thể chọn 30 s. Pipeline nguồn chọn cửa sổ 30 s, quét bước 5 s. Với 30 s RR=1000 ms rồi 30 s RR≈666,667 ms, mean/SDNN toàn phiên là 0,8/0,164399 s, trong 30 s cuối là 0,666667/≈0 s. Đây là khác đầu vào, không chứng minh xác suất AF thay đổi bao nhiêu.

**Hướng triển khai chọn cho phiên 60 s:** xét các cửa sổ `[0,30)`, `[5,35)`, …, `[30,60)` so với start. Giữ peak timestamp và sample/integrity/SQI theo từng cửa sổ, chỉ lấy interval có cả hai peak bên trong và trong cùng continuity segment. Chọn cửa sổ đạt QC có nhiều RR hợp lệ nhất, hòa chọn cửa sổ sớm nhất, để khớp quy tắc chọn của offline bridge. Ghi start/end/count/reasons của cửa sổ được chọn. Nếu không có cửa sổ đủ sạch, trả trạng thái thiếu dữ liệu/chất lượng tương ứng, không suy luận từ phần dữ liệu sót lại.

Đây là chính sách chọn window của pipeline offline hiện tại, không phải thuộc tính nhúng trong model weights. Nếu đổi sang latest-30s để đơn giản hóa streaming, phải ghi đó là thay đổi chính sách, cập nhật golden-window comparison trước khi tuyên bố tương đương. Không được đổi toàn bộ measurement xuống 30 s vì Stress cần 60 s.

Đề xuất cấu trúc: window builder ECG riêng + timestamped peaks/continuity IDs + thống kê theo cửa sổ hoặc bucket; không bắt buộc lưu toàn bộ 30.000 raw samples trong heap. Giữ DSP chạy liên tục, không reset filter mỗi window. Gate ECG phải dùng SQI của chính window 30 s; diagnostics tổng phiên giữ riêng. SpO2 4 s, Stress 60 s và ECG 30 s phải có window metadata riêng; trước mắt có thể giữ gate SpO2 toàn phiên nếu đánh dấu rõ là conservative, không tuyên bố đó là SQI riêng 4 s.

**File:** new ECG window builder, FeatureBuilder, types/window metadata, rhythm engine quality gate, logger/telemetry.

**Nghiệm thu:** chọn đúng cửa sổ, tie-break, loại RR vượt biên/gap, noise ngoài window không nhiễm window đã chọn, noise trong window bị gate; đúng 9 feature từ cùng RR so với Python. Xem T05.

### A06 — P2: giữ đúng nghĩa feature và bổ sung kiểm chứng inference thực

**Đã tốt:** header Stress chạy 60 vector test nguồn và khớp Python từ model.json (sai số double đo được 0; không có label mismatch giữa hai implementation). Feature order/units ECG đúng; target build gọi model thật và build thành công. Các dữ kiện này chưa chứng minh DSP ra đúng peak trên phần cứng.

**Thiếu:** native define `PPGFW_NATIVE_TEST` làm ECG model adapter trả về sớm. Native test không chạy Edge Impulse. Fixture trên bo hiện chỉ in output, không đối chiếu xác suất kỳ vọng. Firmware DSP causal đơn giản khác `sosfiltfilt` và detector Python nguồn; `tin_hieu` cũng có cutoff/rate khác, API combined sample rate. Không thể coi chúng tương đương bằng việc copy file.

**Hướng triển khai:** giữ nguyên model/scaler, kiểm thử hai tầng: (1) cùng timestamp peak/RR → feature phải khớp oracle; (2) cùng raw capture → so sánh peak/SQI/features và báo khác biệt DSP. Port/adapt Task 2 thành hai pipeline theo modality/rate nếu bằng chứng replay cho thấy cần; không nuôi cả ECG500 và PPG200 vào API chung giả định một fs, không nội suy để che mismatch. Nếu thay DSP/QC, bump version và giữ replay trước/sau.

Thêm target/host integration test thực sự gọi model ECG được vendoring. Thử host build phù hợp với SDK/porting; nếu không hỗ trợ thì fixture chạy trên ESP32-S3 là bước bắt buộc còn mở, không tạo fake AF labels. Golden probability phải đến từ cùng model export chạy thật và có provenance, không hardcode lại kết quả implementation đang sửa làm oracle.

BPM hiển thị dùng chính PPI hợp lệ được chọn cho Stress (median), hoặc ghi rõ nếu dùng cửa sổ/QC khác. Hiện helper chung chấp nhận [250,2000] ms và 1 interval, khác helper PPG nguồn [333,33,1500] ms và tối thiểu 4 interval. Tách policy PPG/ECG thay vì đổi helper chung làm hỏng nhánh ECG.

**Legacy ở gốc:** `src/main.cpp:421` dùng số IBI cho `beat_count`, và lọc interval trước mẫu số valid ratio. Không cần sửa root app để hoàn tất sửa `firmware/`; giữ README chỉ rõ legacy. Nếu tiếp tục hỗ trợ legacy, sửa riêng theo cùng contract và test trước khi gọi nó tương thích. Không tự xóa/gộp root vào firmware.

## 4. Trạng thái và kết quả phải giữ đúng nghĩa

- `kLowO2RuleValidated=false` và `clinically_validated=false` giữ nguyên. SpO2 Ok là điều kiện phần mềm, không bật cảnh báo LowO2 để ép kết quả hệ thống.
- Giữ `non-AF` khác Normal. Không sửa `negative_conclusive` chỉ để màn hình xuất hiện NORMAL.
- Thêm reason cụ thể cho FIFO/I2C/timing/window nếu cần; version schema khi thay layout binary. Cập nhật converter/replay và snapshot consumers cùng commit thay đổi schema.
- Logs phải cho biết model/scaler/DSP/SQI version, window start/end, raw/valid interval count, gap/reset/drop count và inference latency. Không gắn SQI 60 s lên feature ECG30 như cùng cửa sổ.
- Kiểm tra UI/MQTT thực sự xuất nhánh Stress, AF và trạng thái SpO2: OLED hiện chưa có nhãn Stress; MQTT hiện có `stress_status` nhưng thiếu xác suất/nhãn Stress. Đây là phần hiển thị/logging chưa đủ quan sát, tách khỏi tương thích numerical.

## 5. Thứ tự commit đề xuất và checklist bàn giao

1. `test: reproduce acquisition and AI window failures` — đưa regression tests vào firmware test, có thất bại trên baseline cho A01–A05.
2. `fix(acquisition): preserve FIFO samples and real ECG timestamps` — A01/A02; pin dependency và kiểm tra driver API.
3. `fix(timing): align continuous samples and measurement deadlines` — A03/A04, gap/reset/warmup semantics.
4. `fix(features): build independent ECG windows and matching quality` — A05, đồng bộ window metadata/schema consumers.
5. `test(ai): verify features and actual model inference` — A06, regression against pinned AI and explicit missing board evidence.
6. `docs: record compatibility results and hardware work remaining` — status/readme/report, telemetry/UI nếu cần commit riêng.

- [x] A01 direct FIFO + fake-bus fault-path regression đạt; electrical/bench còn mở.
- [x] A02 không backfill, actual timestamps và host dropout/queue tests đạt; rate/jitter trên bo còn mở.
- [x] A03 poll jitter không reset giả; gap/duplicate/rate/config handling có regression.
- [x] A04 microsecond deadline, warmup DSP, half-open window, cancel/restart và raw replay đạt.
- [x] A05 candidates ECG30/PPG60, same-window SQI, tie/count/rate-change/gap tests đạt.
- [x] A06 numerical parity + host inference ECG thật có bằng chứng; golden độc lập và raw capture thật còn mở.
- [x] Build đủ default/MQTT/raw-log/fixture PASS trên source cuối; native 40/40 và synthetic replay có bằng chứng.
- [x] Log/schema/README/report cập nhật; schema 2/3 round-trip đã kiểm tra.

Các ngưỡng SQI vật lý, sai số peak chấp nhận trên raw capture và jitter budget trên bo chưa có calibration đóng băng. Ghi chúng là mục cần đo/chốt, không tự nới threshold để test pass. Tuy nhiên vẫn hoàn thành mọi sửa phần mềm và test synthetic có thể làm độc lập.

## 6. Lệnh thực hiện kế hoạch ban đầu (lịch sử)

> Đọc AGENTS.md, IMPLEMENTATION_PLAN.md và VALIDATION.md. Kiểm tra phiên bản hiện tại, tái hiện A01–A05, rồi sửa firmware theo thứ tự commit đã đề xuất. Giữ pinout/sampling/model pin và mã root legacy. Hoàn thành kiểm thử phần mềm, build bốn profile, ghi rõ kết quả inference ECG nào thực sự chạy và phần nào còn cần bo. Không báo hoàn tất tương thích phần cứng chỉ từ build/native. Cập nhật checklist với commit và evidence tương ứng.

Xem [VALIDATION.md](VALIDATION.md) và [evidence/](evidence/) để chạy lại bằng chứng. Các probe là dụng cụ quan sát baseline; exit 0 của runner không có nghĩa firmware đã tương thích.
