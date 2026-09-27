# ThingsBoard và luồng OLED 0.5.0

## Màn hình và hai nút

OLED SH1106 128×64 dùng sáu dòng tối đa21 ký tự, tiếng Việt không dấu để giữ
font nhỏ và rõ. Không thay pinout, model hoặc tần số PPG200Hz/ECG500Hz.

| Trạng thái | Nội dung / thao tác |
|---|---|
| Khởi động | Kiểm tra cảm biến rồi vào màn hình sẵn sàng; lỗi thì nút1 thử lại |
| Sẵn sàng | Hướng dẫn đặt ngón tay + điện cực, giữ yên60s, báo OLED sẽ tắt khi đo; nút1 bắt đầu |
| Chờ tiếp xúc | Báo riêng ngón tay và ECG, nút2 hủy; quá120s về chờ |
| Ổn định / đo | Tắt OLED và Wi-Fi; vẫn thu đủ warmup3s + cửa sổ60s; nút2 hủy |
| Xử lý | Dừng thu, bật OLED, báo đang tính kết quả trước khi chạy inference |
| Kết quả1/3 | HR ECG, pulse PPG, SpO2 ước lượng và trạng thái gửi; chỉ số không hợp lệ hiển thị `--` |
| Kết quả2/3 | Stress, sàng lọc AF, yêu cầu đo lại nếu cần; ghi rõ không phải chẩn đoán |
| Kết quả3/3 | Đang kết nối/gửi, máy chủ đã nhận, chờ gửi lại hoặc kết quả mới chưa được giữ; số kết quả đang chờ |

Ở màn hình kết quả, **nút2 đổi trang**, nút1 đo lại. Chuyển trang gia hạn thời gian
xem; sau45s không thao tác tự về chờ. Ở chờ, nút2 gửi lại các kết quả còn trong
hàng đợi. Bấm đo trong lúc đang gửi sẽ tắt mạng trước khi thu tín hiệu.
OLED tắt sau15s không thao tác ở chờ; nhấn nút đánh thức và thực hiện chức năng
tương ứng. Light sleep vẫn chỉ áp dụng ở Idle khi radio và cảm biến đã dừng.

Nhãn NonAf không đổi thành Normal. Nhãn cũ không được hiện nếu status inference
không hợp lệ; SpO2 NaN/Inf cũng hiển thị `--`. Chưa có phép đo hợp lệ thì không
dùng dữ liệu fixture để điền số lên màn hình kết quả production.

## Kết nối ThingsBoard

Đích đã được người dùng cung cấp: `https://tb.trankhoinguyeniot.io.vn`.
Thiết bị dành cho dự án: **EdgeAI-PPG-01**, ID `478b71b0-ba35-11f1-b659-cb247c6ec264`.
Không sửa các thiết bị demo hoặc thiết bị dự án khác trong tenant.

Profile `esp32-s3-devkitc-1-thingsboard` dùng Device HTTP API qua HTTPS443:
`/api/v1/<device-token>/telemetry`. Cổng MQTT1883/8883 không kết nối được trong
lượt kiểm tra này; cổng443 và REST API hoạt động. Profile MQTT cũ vẫn dành cho
broker LAN; không gửi token qua TCP1883 đến máy chủ Internet này.

TLS xác minh hostname, thời hạn chứng chỉ và CA **GTS Root R4** tải từ Google
Trust Services. Chứng chỉ máy chủ quan sát được có issuer WE1. Không dùng
`setInsecure` hoặc tự theo redirect. Đồng bộ SNTP chỉ bắt đầu sau khi đo xong;
nếu không có giờ hợp lệ, không gửi HTTPS và vẫn chịu giới hạn phiên kết nối.
Khi CA của máy chủ đổi cần cập nhật trust anchor rồi kiểm chứng lại.

Mỗi phiên kết nối có budget30s (kiểm tra giữa các bước mạng; mỗi lời gọi HTTP
cấu hình timeout1s), không tự reconnect vô hạn. HTTP chạy chế độ async; chỉ
HTTP200 mới xóa bản tin khỏi hàng đợi. MQTT cũ vẫn chỉ xóa sau PUBACK QoS1.
Gửi thành công hoặc hết thời gian thì tắt Wi-Fi. Gặp lỗi giữ kết quả để thử lại
bằng nút2 ở Idle hoặc sau lần đo mới. Hàng đợi4 bản tin trong RAM, mất khi tắt
nguồn; đầy hàng đợi phải báo kết quả mới chưa lưu/gửi, kể cả các bản cũ gửi xong.
Không có bảo đảm exactly-once nếu máy chủ đã nhận nhưng phản hồi bị mất.

`timestamp_us` là thời gian monotonic của phiên firmware; ThingsBoard gắn thời
gian nhận vì thiết bị chưa dùng RTC DS3231. Không coi thời gian nhận là thời gian
đo khi gửi lại dữ liệu cũ. Bản0.5.0 chưa sửa DS3231 NACK hoặc bộ đếm1 slot ECG
lúc start được ghi trong báo cáo kiểm thử bo trước đó.

## Cấu hình riêng và kiểm thử

SSID, mật khẩu Wi-Fi và **token thiết bị** nằm trong
`firmware/include/config/network_secrets.h`, bị Git bỏ qua. Tài khoản tenant
không đưa vào firmware. Không commit file riêng, log chứa token hoặc binary có
nhúng thông tin đăng nhập. ESP32-S3 cần Wi-Fi2.4GHz; không dùng SSID chỉ phát5GHz.

Các phép thử có thể chạy lại:

```text
python firmware/tools/ui_test/run.py
python firmware/tools/power_test/run.py
python firmware/tools/serialization_test/run.py
pio test -e native                  (trong firmware/)
```

Kiểm tra UI dùng cùng hàm tạo sáu dòng production: giới hạn ký tự, trạng thái
gửi, không báo đã nhận khi chưa ACK, status/NaN/Inf, NonAf và thời gian xem.
Kiểm tra mạng giả lập Wi-Fi/ESP-MQTT/HTTPS: ACK, EAGAIN,401/403/5xx/redirect,
TLS failure, timeout đồng bộ giờ, retry, ngắt để đo và hàng đợi đầy.
Không coi mock TLS failure là phép tấn công chứng chỉ trên bo thật.

ThingsBoard đã nhận bản tin **thử kết nối từ máy tính** và đọc lại đúng qua API,
các trường `integration_test`, `test_origin=host_https`, `firmware_target=0.5.0`.
Đây không phải dữ liệu người đo, cũng chưa chứng minh ESP32 kết nối Wi-Fi được.
Kết quả build và trạng thái nạp thực tế ghi trong `VALIDATION.md` cùng thư mục.

Tài liệu giao thức: [HTTP telemetry](https://thingsboard.io/docs/reference/http-api/telemetry/),
[REST API](https://thingsboard.io/docs/reference/rest-api/),
[GTS Root R4](https://pki.goog/repo/certs/gtsr4.pem).
