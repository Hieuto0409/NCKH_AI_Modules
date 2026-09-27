# Kiểm chứng OLED và ThingsBoard 0.5.0 — 27/09/2026

Source bắt đầu từ `b773395`. Chỉ thay giao diện, trạng thái gửi, transport HTTPS,
thời gian xem kết quả và công cụ kiểm thử; giữ model pin, scaler, DSP, pinout,
tần số/cửa sổ thu. Không sửa hai phát hiện ECG start-slot và DS3231 của lượt trước.
Các hash và tổng hợp nằm trong [evidence.json](evidence.json).

## Đã kiểm chứng

- **42/42 native tests PASS** trên source mới.
- **OLED host regression PASS**: sáu dòng không quá21 ký tự ASCII, chỉ số
  invalid/NaN/Inf hiển thị thiếu dữ liệu, không biến NonAf thành Normal, status
  không hợp lệ chặn nhãn cũ, không báo máy chủ đã nhận khi chưa ACK, gia hạn và
  hết hạn màn hình kết quả. Test gọi cùng frame builder và state machine production.
- **Mạng host PASS**: tám kịch bản MQTT cũ; HTTPS ACK/EAGAIN,301/302/401/403/500/503,
  lỗi TLS, thiếu giờ/timeout/retry, ngắt khi đo, hàng đợi đầy (kết quả mới bị từ
  chối vẫn được báo kể cả bản cũ gửi xong), lỗi init/header. Cấu hình trống giữ RF off.
- **Serialization PASS**: JSON765byte, binary18records, schema/version, null,
  cửa sổ, nhãn, xác suất, CRC. Không đổi payload sức khỏe trong lượt này.
- **Máy tính → ThingsBoard thật PASS**: đăng nhập HTTPS, tạo `EdgeAI-PPG-01`,
  gửi telemetry thử không chứa chỉ số sức khỏe, nhận HTTP200 và đọc lại đúng
  `integration_test=true`, `test_origin=host_https`, `firmware_target=0.5.0`.
- **CA thực PASS trên máy tính**: TLS tới đúng hostname với trust store chỉ
  chứa GTS Root R4 của firmware, bật kiểm tra hostname và thời hạn; không dùng
  bộ CA hệ điều hành để che thiếu trust anchor trong phép thử này.
- **Nạp bo thật qua VS Code PASS**: bản `esp32-s3-devkitc-1` offline0.5.0,
  esptool xác nhận bốn hash vùng ghi, SELFTEST PPG/ECG ok, PSRAM=yes. Capture25s
  có một boot0.5.0 và24 dòng DIAG Idle bằng0; không thấy crash/brownout/reset lặp.
  Binary offline không chứa token ThingsBoard (đã kiểm tra trực tiếp).

## Phạm vi chưa hoàn tất

**ESP32 → Wi-Fi → ThingsBoard: NOT RUN.** Mạng máy tính đang dùng là5GHz; S3
cần2.4GHz. Đọc mật khẩu profile2.4GHz đã lưu bị cơ chế duyệt tự động chặn vì cần
quyền trích xuất mật khẩu cụ thể; chưa đọc được mật khẩu. Đang chờ xác nhận hoặc
mật khẩu do người dùng cung cấp. SSID/password trong cấu hình riêng vẫn trống.
Token thiết bị đã lưu riêng, không có tài khoản tenant trong firmware.

Bản HTTPS đã build không đồng nghĩa đã kết nối từ bo; bản **đang cài là offline**.
Sau khi có cấu hình mạng được phép dùng: build lại profile ThingsBoard, cập nhật
manifest cục bộ theo binary mới, kiểm thử bản tin đánh dấu `test_origin=esp32`
không chứa số đo người, đọc lại ThingsBoard và xác nhận radio tắt sau ACK/timeout.
Không gọi bản tin từ máy tính là bản tin của ESP32.

Chưa có người quan sát trực tiếp màn OLED, nhấn hai nút trên bo, thử một phiên
đo tiếp xúc hợp lệ, xác nhận gửi lại khi mạng mất, GPIO wake hoặc đo dòng/nhiễu.
Không coi layout/string test là nghiệm thu hình ảnh hay luồng nút trên phần cứng.
Giới hạn USB/điện cực và kiểm chứng lâm sàng ở báo cáo bo trước vẫn còn.

Build logs, raw UART và session API riêng giữ local trong
`firmware/.pio/verification/thingsboard/`, bị Git bỏ qua. Không commit binary có
token, session, mạng đã lưu hoặc raw thông tin nhận dạng bo. Firmware build dùng
Arduino ESP32 core và Xtensa toolchain đang cài như lượt0.4.0.

## Build trên toolchain ESP32

**5/5 PASS.** Các profile mạng build với token thiết bị cục bộ nhưng SSID/password
trống; không coi đây là bản đã cấu hình Wi-Fi hoàn chỉnh.

| Profile | Static RAM (byte) | Flash (byte) |
|---|---:|---:|
| esp32-s3-devkitc-1 | 81736 | 414805 |
| esp32-s3-devkitc-1-thingsboard | 114024 | 990905 |
| esp32-s3-devkitc-1-mqtt | 113504 | 967873 |
| esp32-s3-devkitc-1-raw-log | 80048 | 401337 |
| esp32-s3-devkitc-1-fixture | 22264 | 330441 |

Cảnh báo macro `EI_PORTING_ARDUINO` của SDK đã tồn tại từ trước; không thay file
sinh tự động. RAM trên là static, chưa phải peak heap của TLS khi chạy trên bo.
