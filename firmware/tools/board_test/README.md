# Kiểm thử bo từ VS Code

Với source0.5.0, dùng `--manifest docs/thingsboard-oled/evidence.json` (đường dẫn
tính từ repo root) và `--log-dir firmware/.pio/verification/thingsboard`. Manifest
0.4.0 được giữ làm bằng chứng lịch sử và sẽ từ chối binary mới. Sau khi thay cấu
hình riêng, hash binary có thể đổi: build/kiểm chứng lại trước khi tạo manifest
cục bộ mới. Xem [kết quả0.5.0](../../../docs/thingsboard-oled/VALIDATION.md).

Kết nối từ bo đã được kiểm chứng thêm bằng profile `esp32-s3-devkitc-1-network-diag`;
xem [BOARD_NETWORK.md](../../../docs/thingsboard-oled/BOARD_NETWORK.md). Profile này
gửi metadata thử với cảm biến dừng và phải được thay bằng production ThingsBoard
sau khi thử. Manifest của lần có Wi-Fi là `docs/thingsboard-oled/board-network-evidence.json`.
Không dùng hash bản chưa cấu hình Wi-Fi để nạp bản có cấu hình riêng.

Mở thư mục `firmware/` trong VS Code. Công cụ `flash_and_capture.py` dùng Python
của PlatformIO (có pyserial), esptool và các binary đã build. Nó kiểm tra SHA256
`firmware.bin` với manifest trước khi ghi; esptool kiểm tra các vùng flash đã ghi.
Không erase toàn chip, không xóa NVS và không nạp secrets. Không mở Serial Monitor
khác cùng lúc. Capture sẽ reset bo qua DTR/RTS, kể cả `--capture-only`.

`tasks.example.json` có thể chép vào `.vscode/tasks.json` khi mở `firmware/` làm
workspace trên Windows. Chọn **Tasks: Run Task** rồi chọn tác vụ ESP32. Python
được tìm tại `${env:USERPROFILE}/.platformio/penv/Scripts/python.exe`; chỉnh lại
nếu PlatformIO được cài ở nơi khác. COM6 là cổng đã kiểm chứng ngày27/09/2026,
phải xác nhận lại cổng/bo trước lần nạp khác.

1. Build profile fixture và production bằng `tools/build_windows.ps1` nếu cần.
2. Chạy fixture: xác nhận `OFFLINE TEST COMPLETE` và đọc từng status/kết quả.
3. Build profile `esp32-s3-devkitc-1-board-diag` để kiểm tra phần cứng. Tác vụ
   chẩn đoán dùng manifest lịch sử trong `docs/board-validation/evidence.json`.
   Nếu build mới khác hash, dừng và kiểm chứng build/source trước khi tạo manifest
   mới; không bỏ kiểm tra hash chỉ để nạp được.
4. Chạy chẩn đoán, đọc `BOARD DIAGNOSTICS COMPLETE` cùng toàn bộ kết quả. Từ
   COMPLETE chỉ có nghĩa chương trình chạy tới cuối, **không có nghĩa mọi test đạt**.
5. Luôn nạp lại profile production cuối cùng, xác nhận đúng phiên bản và SELFTEST.

Helper mặc định lưu log vào thư mục local có ngày `board-2026-09-27` và ghi đè log
cùng profile. Sao lưu log trước khi lặp lại. Byte ROM boot115200 có thể không đọc
được ở đầu capture921600; đối chiếu các dòng runtime sau đó. Timeout capture
không tự đánh giá PASS/FAIL; cần đọc log và kiểm tra kết quả, lỗi/reset và số mẫu.
Manifest production/fixture mặc định là `docs/power-and-noise/evidence.json`.

Thông số ghi dùng flash16MB, DIO80MHz và bootloader/application của từng build;
đã chạy được trên bo S3 N16R8 này. Không áp dụng cho bo khác mà không kiểm chứng.
Không dùng profile chẩn đoán để kết luận độ chính xác sinh học hoặc pin.
