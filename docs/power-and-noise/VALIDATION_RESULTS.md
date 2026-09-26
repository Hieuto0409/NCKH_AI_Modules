# Kết quả kiểm chứng firmware 0.4.0

Ngày 27/09/2026. Nguồn bắt đầu từ `f115c38`; kết quả này thuộc source có SHA-256
`2848419a70e00cf9f1476031711a5b0428b8d74979f19bfa554f46f086174d90`. Phạm vi digest và SHA-256 binary ghi trong
[evidence.json](evidence.json). Các binary/cache không đưa lên Git.

## Đã chạy

- `pio test -e native`: **42/42 PASS**, gồm hai regression mới: ECG dừng/resume
  không tạo mẫu giả/đếm thời gian ngủ thành mất mẫu; giới hạn chờ tiếp xúc vẫn hết
  hạn khi trạng thái Warmup/ContactWait thay đổi do mất tiếp xúc.
- `python firmware/tools/power_test/run.py`: **8 kịch bản PASS** trên production
  `mqtt_client.cpp` với WiFi/ESP-MQTT giả: RF off khi đo/khởi động; QoS1 và ACK
  đúng/sai id; timeout không retry vô hạn; millis wrap; ngắt để đo rồi gửi lại;
  đầy queue; broker error; enqueue failure; init failure được nhóm theo 8 scenario.
  Thêm cấu hình trống: PASS, không bật radio, báo từ chối yêu cầu gửi.
- `python firmware/tools/serialization_test/run.py`: PASS, JSON765 byte,
  18 binary records, raw schema2/feature schema3, window/label/probability/null và
  CRC đúng. Publisher nay tạo/queue JSON ngay cả khi chưa kết nối.
- Build đầy đủ bằng `firmware/tools/build_windows.ps1` trên Arduino ESP32 core
  `4.20017.260907+sha.dcc1105b`, Xtensa GCC8.4.0: **4/4 PASS**.
- Diff kiểm tra: không đổi thư viện model, scaler, pinout hoặc cấu hình sampling.
  Không chạy lại golden/model oracle vì không đổi thuật toán/feature/model;
  bằng chứng trước đó vẫn nằm ở `docs/ai-compatibility/`.
- `git diff --check` đạt; `network_secrets.h` được Git ignore.

| Profile | Static RAM (byte) | Flash (byte) | Build |
|---|---:|---:|---|
| esp32-s3-devkitc-1 | 81728 | 413293 | PASS |
| esp32-s3-devkitc-1-mqtt | 113488 | 966125 | PASS |
| esp32-s3-devkitc-1-raw-log | 80040 | 400897 | PASS |
| esp32-s3-devkitc-1-fixture | 22264 | 330441 | PASS |

Static RAM chưa bao gồm heap Wi-Fi/MQTT, task stack và peak allocation của inference.
Cần đo minimum free heap/high-water trên bo. SDK model vẫn có warning macro
`EI_PORTING_ARDUINO` đã tồn tại; không sửa code sinh tự động để che warning.
Full build log giữ local tại `firmware/.pio/verification/power-builds.txt`.

## Chưa chạy / giới hạn

Không flash bo, không đo dòng/nhiễu/rail, không kiểm chứng GPIO wake, OLED power-save,
MAX shutdown/resume, ADC jitter hoặc broker ThingsBoard thật trong lượt này.
Host mocks chỉ xác nhận điều khiển API và trạng thái; không chứng minh RF thực đã tắt,
PUBACK qua mạng thật hoặc thời lượng pin. Golden ECG độc lập và raw annotations
vẫn là hạng mục mở từ bản0.3.0.

Đọc [REVIEW.md](REVIEW.md) để biết sơ đồ nguồn, rủi ro, giới hạn hàng đợi RAM,
TCP1883, timestamp, và ma trận đo phần cứng cần hoàn thành.
