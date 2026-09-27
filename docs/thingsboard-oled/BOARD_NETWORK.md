# ESP32 → ThingsBoard thực — 27/09/2026

Đã cấu hình Wi-Fi riêng sau khi người dùng xác nhận quyền đọc mật khẩu đã lưu.
Mật khẩu, SSID và token chỉ nằm ở cấu hình local bị Git bỏ qua. Không đưa tài
khoản tenant vào firmware. Nạp và chạy bằng các process task của VS Code, COM6.

## Kết quả

- ESP32 nhìn thấy mạng mục tiêu ở **kênh6, RSSI−62dBm** trong lượt scan chẩn đoán.
- MAX30102 đã shutdown, không khởi động task ECG trong khi thử mạng.
- Gate `upload_allowed=false`: bản tin vẫn nằm trong RAM, `radio=0`, Wi-Fi mode0.
- Retry chủ động: kết nối Wi-Fi khoảng4,9s, đồng bộ giờ xong khoảng6,2s, nhận
  **HTTP200 sau8798ms**, `ack=1`, hàng đợi còn0. TLS dùng CA/hostname/time check
  của transport production, không tắt xác minh chứng chỉ.
- Sau ACK: **radio=0, Wi-Fi mode0**. Heap còn281728byte, min heap203312byte trong
  **profile chẩn đoán**; không áp dụng con số này cho production có model AI.
- Gửi bản tin thử thứ hai rồi gọi gate dừng ngay khi đang kết nối: hàng đợi còn1,
  ACK vẫn1, radio/mode đều0. Chứng minh hủy kết nối giữ bản tin trong RAM;
  không phải phép thử nhấn nút vật lý hoặc mất mạng ở mọi giai đoạn.
- API ThingsBoard đọc lại đúng `test_origin=esp32`,
  `tb_board_probe_id=ppgfw-board-20260927-01`, `integration_test=true`,
  `firmware_target=0.5.0`, timestamp nhận `1790489092239`.
  Key `cancelled_network_probe` chỉ trả placeholder `value:null`, không có giá trị
  đã gửi. Khi kiểm chứng key vắng trên server này, kiểm tra `value`, không chỉ
  kiểm tra mảng rỗng. Probe là metadata kết nối, không có số đo sức khỏe.

Nguồn vẫn là model pin và firmware0.5.0. Lượt này thêm profile chẩn đoán riêng
`esp32-s3-devkitc-1-network-diag` và trường đọc mã HTTP cuối cho chẩn đoán;
không thêm gửi test tự động vào startup production.

## Bản cuối trên bo

Đã nạp lại **`esp32-s3-devkitc-1-thingsboard`**, gồm AI, OLED và upload HTTPS sau
khi đo. Profile chẩn đoán không còn là bản chạy trên bo. Firmware chính khởi động
với SELFTEST PPG/ECG ok, PSRAM=yes, cấu hình upload sẵn sàng; không có bản tin chờ
ở Idle. Hash và UART đã lọc thông tin riêng nằm trong
[board-network-evidence.json](board-network-evidence.json).

Luồng chính vẫn tắt Wi-Fi trong contact/warmup/acquisition, chỉ gửi sau kết quả,
timeout30s và tắt radio sau ACK/timeout. Nút2 ở Idle retry dữ liệu còn trong RAM;
không giữ được hàng đợi qua mất nguồn. Khi đổi mạng cần build/nạp lại cấu hình
riêng; không dùng mạng5GHz cho ESP32-S3.

## Kiểm chứng phần mềm và phần chưa chạy

- Build chẩn đoán và production ThingsBoard có cấu hình riêng: **2/2 PASS**.
  Chẩn đoán: static RAM54816, flash901717byte. Production: static RAM114032,
  flash990929byte. Binary có chứa cấu hình riêng nên không commit.
- Chạy lại host upload regression: tám kịch bản MQTT,13 kịch bản HTTPS và
  unconfigured-radio-off đạt. Bộ42 native, UI và serialization ở lượt trước vẫn
  là bằng chứng cho phần không đổi; không gọi chúng là đã chạy lại trong lượt này.
- Chưa kiểm chứng toàn bộ chu kỳ đo người → AI → upload, quan sát OLED/nút thật,
  GPIO wake, mức dòng/RF thực, mất mạng mọi giai đoạn, long-run hoặc độ chính xác
  lâm sàng. DS3231 NACK và ECG đếm1 slot lúc start vẫn chưa sửa.

## Chạy lại

Build profile chẩn đoán với config riêng, ghi hash vào manifest local rồi nạp
bằng `tools/board_test/flash_and_capture.py --environment
esp32-s3-devkitc-1-network-diag --manifest <manifest> --log-dir <local-log-dir>`.
Kiểm tra từng dòng NETTEST, ACK/HTTP và readback có timestamp thuộc lượt thử mới;
probe ID cố định không đủ để chứng minh một lần gửi mới. Từ COMPLETE chỉ có
nghĩa chương trình tới cuối, không tự đồng nghĩa PASS. Luôn nạp lại profile
production ThingsBoard sau thử nghiệm. Scan/gửi metadata lúc boot chỉ có trong
profile chẩn đoán, không xuất hiện trong bản sử dụng hằng ngày.
