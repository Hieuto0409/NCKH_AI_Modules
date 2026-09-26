# Báo cáo sửa tương thích firmware–AI 0.3.0

Ngày: 2026-09-26. Repo: `Hieuto0409/EdgeAI-PPG-Screening`, firmware trong `firmware/`.
Baseline code `df46fc1`; kế hoạch lưu tại `078a23b`.
Runtime sửa ở `398c91b`, bổ sung recovery A_FULL ở `49ae13c`;
công cụ host/oracle/schema ở `7ebc65f`. Giữ nguyên model pin
`7d4683581c8c01ff6878bcad31a601aff271f2a0`, pinout và tốc độ PPG 200 Hz / ECG target 500 Hz.

## Kết quả phần mềm

- **A01:** đọc trực tiếp FIFO MAX30102, tối đa 32 mẫu RED/IR, mỗi giao dịch tối đa
  5 cặp (30 byte) để vừa I2C buffer 32 byte. SparkFun khóa 1.1.2 chỉ làm setup;
  không gọi `check()/available()/nextSample()` để lấy dữ liệu. Full/empty dùng pointer
  và A_FULL; rollover tắt. NACK/short read/overflow không tạo mẫu hợp lệ từ bytes thiếu.
  Recovery dừng sensor, xóa pointer/counter và cờ A_FULL còn lưu, khởi động lại; nếu cấu hình bị reset thì
  setup lại. Sensor mất kết nối được thử lại sau 1 s. Poll quá 160 ms làm mất continuity.
  Một outage liên tiếp được gộp đến mẫu hồi phục đầu tiên; số mất không biết chính xác
  được báo là **cận dưới**, không giả làm số đo chính xác.
- **A02:** timer 2 ms đánh thức một FreeRTOS task riêng đọc ADC1. Timestamp là trung
  điểm trước/sau lần đọc thực. Bỏ catch-up loop và lần gọi ADC trong UI loop; tick
  chậm được gộp, slot bỏ lỡ được đánh dấu. Không đọc ADC trong ISR. Queue SPSC giữ
  nguyên dữ liệu khi reset diagnostics. Đây là target 500 Hz, chưa phải xác nhận
  jitter/rate đạt trên bo.
- **A03:** sau lần neo đầu tiên, timestamp PPG tăng theo từng mẫu 5.000 µs; thời điểm
  host poll không tạo gap giả. Fault thật re-anchor và gắn dropout. SpO2 từ chối rate
  khác 200 Hz; duplicate/time reversal bị bỏ, gap/đổi cấu hình reset stream.
- **A04:** state machine, start và deadline dùng cùng đồng hồ 64-bit µs. Window là
  `[start,start+60 s)`. Warmup chạy DSP; bắt đầu phép đo chỉ xóa thống kê của phiên,
  giữ filter/detector đã ổn định. Chờ một mẫu sau biên để chốt local maximum;
  timeout 160 ms tránh treo khi mất sensor. Gap reset DSP và PPI segment.
- **A05:** ECG có 7 candidate 30 s, bước 5 s. Mỗi candidate có sample statistics,
  integrity, peaks và RR riêng; không dùng RR vượt biên hay nối qua gap. Chọn QC tốt
  có nhiều RR hợp lệ nhất, hòa chọn sớm nhất. Timestamp thực có thể làm số mẫu ở biên
  dao động; completeness vẫn dùng ngưỡng SQI cũ, không biến sai khác count này thành
  transport dropout giả. Sample sequence/flags vẫn loại mọi transport gap đã quan sát.
- **A06:** giữ nguyên model/scaler và feature order/units. Pulse BPM lấy median PPI
  đã làm sạch của Stress, tối thiểu 4 interval. ECG HR/HRV lấy từ candidate đã chọn.
  Fusion pulse-arrival không được xuất từ hai cửa sổ khác nhau. Host chạy được ECG
  export thật; golden oracle độc lập và raw capture thật vẫn còn mở bên dưới.

SpO2 metadata phản ánh tối đa 400 cặp cuối = 4 s, nhưng gate chất lượng vẫn dùng toàn
phiên PPG60 một cách bảo thủ. Không gọi đó là SQI độc lập của 4 s. `LowO2RuleValidated`,
`clinically_validated` và tính kết luận âm của `non-AF` vẫn false.

Nguồn thiết kế FIFO: [MAX30102 datasheet, FIFO pointers/status/configuration](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf).
ADC continuous không dùng vì cấu hình 500 Hz nằm dưới giới hạn 611 Hz của core đang
cài; không tự thay rate bằng oversampling mà chưa kiểm chứng.

## Bằng chứng đã chạy

| Phép kiểm tra | Kết quả | Phạm vi |
|---|---|---|
| Unity native | **40/40 PASS** | FIFO, NACK/partial/overflow/stale recovery, random batches, ADC delay 10/20/100 ms, queue overrun, clock/cancel/restart, ECG selection/QC, synthetic raw clean/gap/flat/clipped |
| Stress 60 vector nguồn | Max probability error **0**, 0 label mismatch | So sánh implementation C++ với `model.json`, không phải accuracy lâm sàng |
| Stress feature oracle | 3 bộ peaks, max absolute error **7.11e-14** | Biểu thức feature trích trực tiếp AST Python nguồn |
| ECG feature oracle | 10 bộ RR, max absolute error **2.44e-6** | Float output; tolerance `atol=1e-5`, `rtol=1e-6`, gồm biên RR và pNN50 |
| ECG model host | **3/3 chạy inference thật** | Không định nghĩa `PPGFW_NATIVE_TEST`; SDK/generated model không sửa |
| MQTT/binary round-trip | **PASS** | JSON 765 byte, 18 binary records, window/label/probability/null/schema/CRC |
| Bốn profile ESP32-S3 | **4/4 SUCCESS**, `evidence/repaired-builds.txt` | Biên dịch/link; không flash thiết bị |

Kết quả quan sát sau sửa trong `evidence/repaired-audit.json`:

- 800 mẫu, batch 4, poll 20.001 µs → **400 cặp, 0 reset giả, không NeedData**.
- Recovery A_FULL: regression trên `398c91b` còn nhận nhầm 32 mẫu từ FIFO rỗng;
  `49ae13c` nhận đúng 0 mẫu. Xem `evidence/repaired-fifo-recovery.json`.
- Poll ADC tại 10.000 µs → **1 lần đọc, timestamp 10.000, 5 slot bỏ lỡ**.
- 59.999.500 µs → chưa finalize; đủ 60.000.000 µs → Stress Ready khi đủ peaks.
- Các thư mục vendor vẫn khớp nguồn theo nội dung chuẩn hóa CRLF: Stress 2,
  ResearchSpO2 5, ECG 1.383 file; không sửa model/SDK.

Host ECG với GNU g++ 15.2.0, export SHA-256
`d09ae9c81890ef5cba0907cb0980f01167f86d2cdc83fca6cf701aac6d15f6d3`:

| Input fixture | P(AF) | P(non-AF) |
|---|---:|---:|
| RR đều 1 s, HRV = 0 | 0 | 0.99609375 |
| Vector HRV tổng hợp thứ hai | 0.328125 | 0.671875 |
| Vector HRV tổng hợp thứ ba | 0.66015625 | 0.33984375 |

Input đầy đủ nằm trong `evidence/repaired-ecg-inference.json`. Output int8 có scale
**1/256** trong export: tổng có thể là 255/256 khi bão hòa, nên kiểm tra tổng dùng
1/256 + 1e-7; không đổi xác suất hay chuẩn hóa lại để ép tổng bằng 1. Đây là smoke
inference của model thật, **chưa phải đối chiếu golden probability độc lập**.
Native Unity vẫn stub nhánh ECG; không lấy kết quả native làm bằng chứng inference.

## Quan sát và định dạng dữ liệu

Firmware 0.3.0; raw schema **2**, feature schema **3**, PPG/ECG DSP và SQI config **2**.
Binary frame vẫn 28 byte, thêm record 38–45; xem `firmware/tools/README.md`.
MQTT có window PPG/ECG/SpO2, label/probability/reason từng nhánh; payload bị từ chối
nếu bị cắt và buffer MQTT tăng lên 2.048 byte. OLED thêm nhãn Stress, giữ AF/non-AF
screening và SpO2 estimate. Serial có window, số interval và reason.

Task loop stack đặt 32 KiB vì adapter Stress có nhiều mảng double tạm; static RAM
của build không bao gồm toàn bộ heap/task stack. Cần đo high-water/heap trên bo.
Không thay bộ lọc causal bằng filter offline zero-phase; không sửa code root legacy
hay `tin_hieu/` để giả định chúng đang nằm trong production build.

## Còn phải kiểm chứng trên bo / dữ liệu thực

1. MAX30102: đầy FIFO, brownout, short I2C, rút/cắm lại và cadence thực; xác minh
   RED/IR không tráo, không sample startup cũ. Fake bus không kiểm chứng điện/I2C timing.
2. ECG: rate/jitter thực, lead-off, 10/20/100 ms tải OLED/logger/MQTT, ADC latency,
   queue loss, stack watermark và long soak. Capture actual timestamps và sequence.
3. Golden probabilities độc lập từ cùng export/runtime hoặc fixture trên bo,
   đối chiếu ba vector host và thêm vector nguồn có provenance. Không hardcode output
   host hiện tại thành một oracle được gọi là độc lập.
4. Replay raw capture có fs/channel mapping và peak annotations: đo sai khác peak,
   SQI, features và inference giữa DSP causal và offline. Synthetic clean/gap/flat/clipped
   đã chạy nhưng không thay dữ liệu người thật hoặc so sánh detector đã gán nhãn.
5. Đóng băng jitter budget, ngưỡng SQI và calibration dựa trên phép đo; chưa có
   cơ sở nới SQI, bật LowO2 hoặc tuyên bố độ chính xác lâm sàng.

Không có flash/upload, đo sensor thật, thử broker MQTT thật hoặc kiểm tra OLED trên
bo trong lượt sửa này. Các hạng mục trên là việc tiếp theo, không được đánh dấu PASS.

## Chạy lại

Trong `firmware/`: `pio test -e native`.
Windows: `./tools/build_windows.ps1 -Environment @('esp32-s3-devkitc-1',
'esp32-s3-devkitc-1-mqtt','esp32-s3-devkitc-1-raw-log','esp32-s3-devkitc-1-fixture')`.

Từ repo root:

```powershell
python docs/ai-compatibility/evidence/run_checks.py --reference ../NCKH_AI_Modules-reference --feature-oracle --output audit-results.json
python firmware/tools/host_inference/run.py --output ecg-host-results.json
python firmware/tools/serialization_test/run.py
```

Oracle feature cần NumPy; các tool khác dùng Python standard library và g++.
`--reference` trỏ checkout nguồn đã kiểm tra. Trong lần này HEAD reference `6f584d6`
chỉ khác pin ở README; vendor model và bridge nguồn khớp pin. Báo cáo audit ghi
parent commit cùng digest source đang chạy để không nhầm commit cha với bản sửa.

## Build source cuối

| Profile | Static RAM (byte) | Flash (byte) |
|---|---:|---:|
| esp32-s3-devkitc-1 | 80764 | 395629 |
| esp32-s3-devkitc-1-mqtt | 84508 | 486765 |
| esp32-s3-devkitc-1-raw-log | 79084 | 374773 |
| esp32-s3-devkitc-1-fixture | 22264 | 330345 |

`evidence/repaired-summary.json` lưu digest source và SHA-256 các binary đã build.
Binary/cache không đưa vào Git. Static RAM chưa bao gồm toàn bộ heap/task stack.
