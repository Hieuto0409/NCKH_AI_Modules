# Rà soát nguồn, nhiễu và tiết kiệm pin — firmware 0.4.0

Ngày 27/09/2026. Đối chiếu trực tiếp [FINAL SCHEMATIC](SCHEMATIC-reviewed.jpg),
firmware tích hợp trong `firmware/` và tài liệu hãng. Nền trước sửa: `f115c38`.
Đây là phân tích sơ đồ và kiểm thử phần mềm; chưa có PCB layout, đo oscilloscope,
đo dòng, thử điện cực/cảm biến hoặc ThingsBoard thật. Không kết luận đã loại hết nhiễu.

## Phần cứng đọc được từ schematic

- Pin qua module ghi `TP_4056`, công tắc S1 tạo `VBAT_SW`; sơ đồ không ghi dung lượng
  pin, dòng sạc lập trình hoặc chi tiết power-path/bảo vệ bên trong module.
- Hai AP2112K-3.3TRG1: B1 cấp nhánh 3.3 V cho ESP32, MAX30102, OLED, DS3231;
  B2 cấp `3.3V_AD8232`. EN nối nguồn, không nối GPIO điều khiển.
- Hai nhánh có tụ 100 µF và tụ `104` (100 nF), nhưng ảnh schematic không xác nhận
  khoảng cách thực tới chân nguồn, ESR, loại tụ hoặc đường hồi dòng trên PCB.
- ECG OUTPUT qua R4=1 kΩ, C12=`104` xuống mass, đến GPIO1/ADC1. LO+/LO− ở GPIO5/6.
  Không có chân SDN của AD8232 được đưa về ESP32 trong sơ đồ.
- Đo pin: R2=R3=100 kΩ, C5=`104`, điểm PIN_READ vào GPIO8/ADC1.
- LED L1 qua R1=470 Ω trên nguồn 3.3 V, luôn sáng khi bật S1.
- MAX30102 và DS3231 dùng chung SDA12/SCL13. INT cảm biến không nối.
- OLED dùng SPI phần mềm GPIO38–42; nút về mass GPIO48/47. Bluetooth không được
  firmware sử dụng. DS3231 hiện chưa được firmware đọc để đóng dấu thời gian.

## Rủi ro cụ thể và cách xử lý

| Nguồn | Cơ chế có thể ảnh hưởng | Phần mềm 0.4.0 / việc cần làm trên bo |
|---|---|---|
| Wi-Fi khi đo | RF ghép vào đường analog/điện cực, xung tải gây sụt nguồn/mass; tác vụ mạng tranh CPU | Không khởi động mạng lúc boot; tắt radio trước khi bật thu mẫu; chỉ gửi khi acquisition dừng. Đo RF/nguồn thực để xác nhận. |
| Hai LDO dùng chung pin và GND | Tách LDO giảm một đường nhiễu nguồn nhưng không cách ly điện, không loại ground bounce | Tách đường hồi dòng công suất khỏi đường ECG; giữ ground plane liên tục và bố trí return path hợp lý, không tạo khe mass tùy tiện. Kiểm tra trực tiếp layout. |
| AP2112 khi pin yếu | LDO không nâng áp; mất headroom làm rail 3.3 V tụt, đặc biệt lúc Wi-Fi phát | Datasheet: dropout tại 300 mA điển hình 125 mV, tối đa 200 mV; tại 600 mA 250/400 mV. Không dùng một mức điện áp pin thấp cố định như bảo đảm rail ổn. Đo cả rail và nhiệt khi pin yếu, Wi-Fi yếu. |
| B1 và tụ | Dòng xung ESP32 + MAX30102 LED + OLED; tụ lớn ở xa không thay decoupling gần chân | Kiểm tra transient, tụ gốm sát chân, đường nguồn/return; 600 mA danh định của LDO không phải bằng chứng đủ công suất/nhiệt trên PCB cụ thể. |
| R4/C12 ở ECG | Với 1 kΩ/100 nF, fc lý tưởng ≈1.59 kHz; không phải notch 50 Hz và không đủ để khẳng định chống alias cho fs=500 Hz | Không tự đổi tụ/lọc DSP vì làm đổi feature. Đo phổ analog và raw ADC; cần thiết kế anti-alias theo băng thông ECG và bộ lọc AD8232 thực có trên module. |
| AD8232/điện cực | Nhiễu 50/60 Hz, tiếp xúc kém, điện cực khô, chuyển động, dây dài và RLD | Giữ phát hiện lead-off, clipping, gap, SQI; chưa có notch thích nghi hoặc chứng minh SQI bắt mọi loại nhiễu. Schematic khối module chưa thể hiện đường điện cực/RLD, phải kiểm tra module/layout thực. |
| MAX30102 | Xung LED dùng chung nguồn số, ánh sáng ngoài, rung/ngón tay ép mạnh | Giữ RED/IR 200 Hz, average=1 và dòng LED đã kiểm thử; không giảm dòng LED/rate tùy ý làm giảm SNR. LED và conversion shutdown khi nghỉ. Đánh giá che sáng, tiếp xúc và perfusion. |
| OLED/SPI | Dòng OLED/charge pump và cạnh SPI có thể ghép nhiễu; software SPI gây tải CPU | Power-save và không gửi framebuffer trong Warmup/Measuring. Hiện kết quả sau đo, tắt màn hình khi Idle quá 15 s. |
| I2C/DS3231 | Shared bus/pull-up, stuck bus, nguồn module hoặc pull-up sai mức | Poll MAX30102 mỗi 10 ms, FIFO vẫn giữ mẫu 200 Hz. Không truy cập DS3231 trong đo. Kiểm tra pull-up lên 3.3 V, điện dung bus, lỗi I2C; không suy ra điện trở pull-up từ ảnh này. |
| GPIO8 đo pin | Cùng ADC1; chuyển kênh/đọc pin trong cửa sổ ECG có thể tăng jitter/settling | Hiện chưa có đọc pin/calibration/low-battery gate. Khi thêm chỉ đọc ngoài acquisition, hiệu chuẩn divider/ADC rồi chốt ngưỡng dựa trên tải. Rth≈50 kΩ, RC≈5 ms; không đọc ngay sau thay cấu hình mà bỏ qua settling. |
| Sạc/USB | TP4056 và USB không tạo cách ly điện; ground máy tính/charger có thể đưa nhiễu vào điện cực | Không coi pin bảo đảm cách ly khi USB/sạc còn cắm. Chưa xác minh an toàn điện cực khi nối nguồn ngoài; thử điện tử bằng giả lập tín hiệu, không nối người với mạch chưa kiểm chứng cách ly. |
| Reset/mất pin | Hàng đợi RAM và kết quả sẽ mất; brownout có thể cắt lúc gửi | Không tắt brownout để che sụt nguồn. Bản này không ghi flash/NVS trong lúc đo; lưu bền sau đo là hạng mục tiếp theo nếu cần giữ dữ liệu qua tắt máy. |

Nguồn cho các nhận định trên: [Espressif schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html),
[PCB layout](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html),
[AP2112 datasheet](https://www.diodes.com/assets/Datasheets/AP2112.pdf),
[AD8232 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/AD8232.pdf),
[MAX30102 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf).
Các rủi ro ghép nhiễu là suy luận từ sơ đồ và hướng dẫn hãng, chưa phải nhiễu đã đo được.

## Luồng nguồn đã triển khai

| Giai đoạn | Wi-Fi | Thu mẫu | OLED / CPU |
|---|---|---|---|
| Boot/self-test | Không kết nối | Khởi tạo/kiểm tra rồi nghỉ | Hiện trạng thái |
| Idle | Tắt, trừ phiên gửi kết quả vừa hoàn thành còn trong ngân sách | MAX30102 shutdown; timer ECG dừng | Sau 15 s tắt OLED, light sleep khi radio tắt và nút đã nhả |
| ContactWait | Tắt | Bật lại, bỏ queue/tiếp xúc cũ | Hướng dẫn đặt ngón tay/điện cực; quá 120 s chưa vào đo thì nghỉ |
| Warmup/Measuring | Tắt | PPG 200 Hz / ECG target 500 Hz, warmup và cửa sổ cũ giữ nguyên | OLED power-save; không log diagnostics định kỳ; không sleep hoặc đổi tần số CPU trong đo |
| Tính kết quả | Tắt | Dừng sau khi drain xong cửa sổ/lookahead | Chạy AI và hiển thị |
| Gửi ThingsBoard | Bật theo yêu cầu, một phiên tối đa 30 s từ lúc bắt đầu Wi-Fi | Đã dừng | QoS 1, chờ PUBACK từng bản tin; hết hàng đợi hoặc lỗi/hết hạn thì tắt |

Thời hạn 30 s là deadline của controller; dừng tác vụ mạng có thể thêm thời gian cleanup
của SDK (network timeout cấu hình 1 s), không phải cam kết RF cutoff chính xác từng ms.
Nếu người dùng bắt đầu đo mới, hủy phiên mạng trước khi bật cảm biến. Kết quả chưa được
ACK vẫn ở queue và sẽ được thử lại khi một phép đo mới hoàn tất. Không tự bật mạng lặp
vô hạn trong Idle. Hết thời gian ContactWait/Warmup không rút ngắn cửa sổ đo 60 s.

Light sleep dùng GPIO wake cho nút47/48 và timer dự phòng 1 s; có kiểm tra cấu hình
wake thành công, không ngủ khi nút đang giữ. Không dùng deep sleep với hai chân này:
EXT0/EXT1 của ESP32-S3 cần RTC GPIO0–21. Nguồn RAM được giữ trong light sleep.
Cần thử wake thực, GPIO pull-up, dòng chờ và thời gian phản hồi trên bo.
Nguồn: [ESP-IDF 4.4.7 sleep modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html).

Các profile ghi raw/binary/CSV cố ý giữ acquisition liên tục để thu dữ liệu thử;
không dùng chúng để đánh giá mức tiết kiệm pin của profile vận hành. Profile fixture
chạy dữ liệu giả, không phải profile cảm biến/pin.

## ThingsBoard và giới hạn giao nhận

- Dùng profile `esp32-s3-devkitc-1-mqtt`. Profile mặc định vẫn chạy offline.
- Copy `firmware/include/config/network_secrets.example.h` thành `network_secrets.h`
  cùng thư mục, điền SSID, password, ThingsBoard host và device access token. File thật
  bị Git ignore; không đưa token vào báo cáo/log.
- Topic `v1/devices/me/telemetry`; token là MQTT username, password MQTT rỗng.
- Chuyển từ PubSubClient publish QoS0 sang ESP-MQTT có sẵn trong ESP-IDF, dùng QoS1.
  Chỉ loại bản tin khỏi queue sau sự kiện PUBACK đúng message id.
- Queue RAM tối đa 4 kết quả, mỗi JSON <1536 byte. Khi đầy, giữ các kết quả cũ,
  từ chối kết quả mới và tăng `rejected`; phép đo mới vẫn mở một cơ hội gửi có hạn.
  `UPLOAD pending=... broker_acked=... rejected=...` cho biết tình trạng qua Serial.
- Broker ACK không đồng nghĩa dashboard/rule chain đã xử lý thành công. QoS1 có thể
  giao trùng nếu ACK bị mất hoặc phiên gửi bị ngắt. Queue không tồn tại qua reset/tắt pin.
- `timestamp_us` là thời gian từ khi ESP32 khởi động, KHÔNG phải Unix time. JSON chưa
  dùng DS3231/SNTP; ThingsBoard gắn thời gian nhận. Kết quả gửi muộn cần phân biệt
  thời điểm đo và nhận, không tự coi thời gian monotonic là lịch thực.
- Port1883/TCP hiện dùng mạng LAN tin cậy; chưa triển khai TLS cho gửi qua Internet.
  Không khai rằng có mã hóa hay tự đổi thành MQTTs mà bỏ kiểm tra CA/time.
- Thiếu cấu hình thì radio không bật, `rejected` tăng khi có yêu cầu gửi. Không
  có ThingsBoard host/token thật trong workspace để thử end-to-end.

Nguồn: [ThingsBoard MQTT](https://thingsboard.io/docs/reference/mqtt-api/getting-connected/),
[ESP-MQTT acknowledgement](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/mqtt.html).

## Phần tiêu thụ điện chưa thể tắt bằng firmware

L1/R1 luôn nối rail3.3V: I_LED=(3.3−Vf)/470, ví dụ Vf=2.0 V thì khoảng2.8 mA.
R2+R3 hút I=VBAT/200kΩ, ví dụ16.5–21 µA ở3.3–4.2 V; đây là phép tính theo
linh kiện ghi trên sơ đồ, không phải dòng đã đo. Hai LDO có IQ điển hình55 µA mỗi
con theo datasheet. AD8232 vẫn có nguồn vì không có SDN/EN từ MCU. Dòng DS3231,
LED/LDO phụ trên module và USB-UART nếu có phải đo ở module thực.

Ưu tiên sửa phần cứng sau phép đo: bỏ/điều khiển LED nguồn; đưa SDN AD8232 hoặc
EN B2 về GPIO thích hợp; kiểm tra back-power qua OUTPUT/LO/I2C trước khi cắt rail;
chọn regulator và cấu trúc nguồn theo pin/tải thực, kiểm tra dropout/nhiệt;
chốt power-path và điều kiện sạc khi chạy. Không thay pinout/model trong lượt sửa này.
Không ước lượng số giờ pin khi chưa biết dung lượng hữu dụng và dòng từng trạng thái.

## Ma trận đo bắt buộc trên bo

1. Dòng ở boot, idle OLED-on, idle light-sleep, contact, warmup, measuring, AI,
   Wi-Fi association, MQTT/ACK; tích phân điện tích cho một chu kỳ và đo standby dài.
2. Scope VBAT_SW, rail3.3V và3.3V_AD8232 với probe ground ngắn: pin đầy/trung bình/yếu,
   AP gần/xa, mất AP, sai token, broker mất mạng. Ghi rail minimum, ripple và reset.
3. Dùng nguồn tín hiệu ECG giả lập/raw capture để so sánh RF-off với RF-on ở firmware
   thử riêng; OLED-on/off, cấp pin/nguồn ngoài cách ly, MAX LED-on/off, vị trí dây/anten.
   Ghi PSD50/60Hz, RMS noise, clipping, peak timing, SQI, sample count/jitter/gaps.
4. Đo thực tắt/bật MAX: LED ngừng, FIFO không mang mẫu cũ; resume ECG không đếm
   thời gian ngủ thành mất mẫu; warmup trước cửa sổ mới và hủy/đo lại hoạt động đúng.
5. Xác nhận GPIO47/48 đánh thức light sleep, debounce, giữ nút, cả hai nút, timer
   dự phòng; queue giữ qua sleep, tắt OLED không làm mất khả năng điều khiển.
6. ThingsBoard thật: đúng token/topic, có PUBACK, data lên dashboard, ngắt mạng
   từng giai đoạn, timeout, đầy queue, ACK mất/gửi trùng, bắt đầu đo khi đang gửi.

Chỉ sau các phép đo mới chốt nhiễu chấp nhận, độ ổn định rail, budget dòng và tuổi pin.
Xem `VALIDATION_RESULTS.md` cho kết quả phần mềm của lượt sửa.
