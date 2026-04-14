# TỔNG HỢP TOÀN BỘ FIRMWARE DỰ ÁN ESP32 (BẢN CHI TIẾT)

Phiên bản tài liệu: 1.0
Ngày cập nhật: 2026-04-02
Phạm vi: Tổng hợp kiến trúc và vận hành firmware trong toàn bộ workspace

---

## 1. Mục tiêu tài liệu

Tài liệu này gom toàn bộ phần mềm firmware của dự án vào một nơi duy nhất, mô tả chi tiết theo đúng mã nguồn hiện có, nhằm phục vụ:

- Nắm tổng thể hệ thống trước khi chỉnh sửa code.
- Bàn giao kỹ thuật cho thành viên mới.
- Đối chiếu giữa code, phần cứng và tài liệu thiết kế.
- Làm tài liệu nền cho test, debug, và lên roadmap phát triển.

---

## 2. Bức tranh tổng thể firmware

Dự án đang có nhiều luồng firmware, tương ứng các môi trường build khác nhau trong [platformio.ini](platformio.ini):

- Luồng ECG chính: [src/main.cpp](src/main.cpp) + [src/ad8232.cpp](src/ad8232.cpp) + [src/ui_ecg.cpp](src/ui_ecg.cpp).
- Luồng Data Collector (nhiệt độ + khoảng cách + gửi cloud): [src/collect_mlx_data.cpp](src/collect_mlx_data.cpp) + [src/ui_datacollector.cpp](src/ui_datacollector.cpp).
- Luồng demo/menu đa màn hình: [src/lcd.cpp](src/lcd.cpp) + [src/ui.cpp](src/ui.cpp).
- Luồng test sensor riêng lẻ:
  - [src/max30102.cpp](src/max30102.cpp)
  - [src/mlx90614.cpp](src/mlx90614.cpp)

Firmware dùng nền tảng:

- Vi điều khiển: ESP32 DevKit.
- Framework: Arduino trên PlatformIO.
- UI: LVGL + TFT_eSPI + ILI9341.
- Sensor chính:
  - AD8232 + ADS1115 cho ECG.
  - MAX3010x cho HR/SpO2 (luồng test riêng).
  - MLX90614 (nhiệt độ).
  - VL53L0X (khoảng cách trong luồng thu mẫu).

---

## 3. Cấu hình build và biến thể firmware

### 3.1 Các environment chính

Theo [platformio.ini](platformio.ini), dự án có các environment:

1. MainChạy ECG monitor với UI ECG chuyên dụng.
2. LcdChạy UI menu/dashboard tổng hợp trên LCD.
3. Max30102Chạy test nhịp tim và SpO2 từ MAX30102.
4. Mlx90614Chạy test đo nhiệt độ hồng ngoại.
5. Ad8232Chạy riêng module AD8232/ADS1115.
6. Collect_mlx_data
   Chạy luồng thu mẫu nhiệt độ + khoảng cách + gửi Google Sheets.

### 3.2 Các thư viện nền

Danh sách thư viện lõi lấy từ [platformio.ini](platformio.ini):

- MAX3010x / MAX30100
- Adafruit GFX
- Adafruit ILI9341
- Adafruit MLX90614
- Adafruit ADS1X15
- Adafruit VL53L0X
- PubSubClient
- ArduinoJson
- LVGL
- TFT_eSPI

### 3.3 Cấu hình hiển thị và hệ thống

Các build flag quan trọng:

- Kích hoạt driver ILI9341 qua TFT_eSPI.
- Gán chân SPI cho LCD (MISO/MOSI/SCK/CS/DC/RST).
- Bật cấu hình LVGL include đơn giản.
- Tăng stack vòng lặp lên 32KB bằng CONFIG_ARDUINO_LOOP_STACK_SIZE=32768.
- Tắt log debug hệ thống mức thấp bằng CORE_DEBUG_LEVEL=0.

---

## 4. Kiến trúc phần mềm theo module

## 4.1 Nhóm ECG lõi

- API: [include/ad8232.h](include/ad8232.h)
- Triển khai: [src/ad8232.cpp](src/ad8232.cpp)

Chức năng chính:

- Khởi tạo ADS1115 và kênh I2C.
- Đọc tín hiệu ECG thô từ ADS1115.
- Phát hiện lead-off từ AD8232 qua LO+ và LO-.
- Lọc tín hiệu đa tầng để lấy ECG sạch.
- Phát hiện nhịp tim dựa trên R-R interval.
- Cung cấp API raw, filtered, bpm, lead-state cho UI và phần log.

## 4.2 UI ECG chuyên dụng

- Header: [include/ui_ecg.h](include/ui_ecg.h)
- Code: [src/ui_ecg.cpp](src/ui_ecg.cpp)

Chức năng:

- Vẽ waveform ECG dạng chart LVGL.
- Hiển thị HR dạng số lớn + điểm nháy theo nhịp.
- Hiển thị cảnh báo khi mất điện cực.
- Input từ phím cứng (UP/DOWN/LEFT/RIGHT/ENTER).

## 4.3 Data Collector UI

- Header: [include/ui_datacollector.h](include/ui_datacollector.h)
- Code: [src/ui_datacollector.cpp](src/ui_datacollector.cpp)

Chức năng:

- Màn hình monitor thu thập dữ liệu với panel ID, tiến độ, khoảng cách, nhiệt độ da, nhiệt độ môi trường.
- Popup trạng thái (đo mẫu, gửi cloud, lỗi mạng, thành công).
- Cập nhật live sensor, tiến độ 0/5, trạng thái WiFi.

## 4.4 Luồng thu mẫu và gửi cloud

- Code chính: [src/collect_mlx_data.cpp](src/collect_mlx_data.cpp)

Chức năng:

- Đọc VL53L0X + MLX90614 liên tục.
- Áp chất lượng mẫu nghiêm ngặt trước khi chấp nhận.
- Thu 30 mẫu hợp lệ liên tiếp cho một lượt đo.
- Tính trung bình và gửi dữ liệu lên Google Apps Script bằng HTTP GET.

## 4.5 UI menu tổng quát

- Header: [include/ui.h](include/ui.h)
- Code chính: [src/ui.cpp](src/ui.cpp)
- Entry LCD demo: [src/lcd.cpp](src/lcd.cpp)

Chức năng:

- Menu nhiều màn hình: monitor, ECG, HR/SpO2, Temp, WiFi scan, WiFi pass, Config.
- Điều hướng 2 chiều bằng keypad và group focus của LVGL.
- Dialog xác nhận quay lại menu.

## 4.6 Web config nhúng

- HTML nhúng: [include/web_interface.h](include/web_interface.h)

Chức năng:

- Giao diện web cấu hình WiFi, email, MQTT user/pass.
- UX có validate email, scan WiFi, toast thông báo, lưu cấu hình.

---

## 5. Luồng chạy thực tế theo từng firmware

## 5.1 Luồng Main (ECG monitor)

Từ [src/main.cpp](src/main.cpp):

1. Khởi tạo Serial.
2. Chọn mode chạy bằng macro:
   - Serial Plotter only.
   - LCD only.
   - Dual (khuyến nghị debug).
3. Nếu có LCD thì init TFT + init ECG UI.
4. Init AD8232 module.
5. Trong vòng lặp:
   - Gọi updateAD8232.
   - Lấy raw, filtered, hr, leads.
   - Cập nhật UI ECG.
   - Xuất serial theo cửa sổ peak-hold.
   - Gọi tick LVGL.

Đặc điểm mode Dual:

- Vẫn log serial để plot waveform.
- Đồng thời hiển thị UI ECG trên TFT.
- Hữu ích để kiểm định thuật toán và giao diện cùng lúc.

## 5.2 Luồng Collect_mlx_data

Từ [src/collect_mlx_data.cpp](src/collect_mlx_data.cpp):

1. Setup button input pull-up.
2. Init TFT và LVGL driver flush.
3. Kết nối WiFi.
4. Init I2C và sensor MLX90614 + VL53L0X.
5. Tạo màn hình Data Collector.
6. Loop:
   - Cập nhật live sensor mỗi 100ms.
   - Xử lý phím:
     - ENTER: đo và gửi mẫu.
     - RIGHT: tăng ID.
     - LEFT: giảm ID.
     - DOWN: reset sample count.

Luồng đo khi bấm ENTER:

1. Hiện popup đo mẫu.
2. Thu 30 mẫu hợp lệ liên tiếp.
3. Nếu xuất hiện mẫu lỗi thì reset chuỗi về 0.
4. Tính trung bình 30 mẫu.
5. Hiện popup kết quả.
6. Gửi HTTP GET lên cloud.
7. Tăng bộ đếm mẫu nếu gửi thành công.

---

## 6. Thuật toán xử lý tín hiệu ECG (chi tiết kỹ thuật)

Phần này nằm ở [src/ad8232.cpp](src/ad8232.cpp).

## 6.1 Chuỗi xử lý tín hiệu

Raw ADC -> Median -> Notch50 -> High-pass -> HR-LPF nhánh HR -> Adaptive LPF nhánh hiển thị -> Baseline compensation -> Soft limiter

Ý nghĩa:

- Median: loại spike đơn lẻ.
- Notch 50Hz: giảm nhiễu điện lưới.
- High-pass: loại trôi nền.
- HR LPF: nhánh riêng để phát hiện nhịp ổn định hơn.
- Adaptive LPF: khi QRS dốc thì giảm lọc để giữ đỉnh.
- Baseline compensation: giữ waveform nằm trung tâm ổn định.
- Soft limiter: tránh bẹt đỉnh do clipping cứng.

## 6.2 Cấu hình lấy mẫu và ADC

- SAMPLE_RATE: 250Hz.
- ADS1115 data rate: 860 SPS.
- Gain ADS1115: GAIN_ONE (±4.096V).
- Quy đổi: 0.125mV/bit.

## 6.3 Phát hiện nhịp tim

Cơ chế:

- Dùng local peak trên tín hiệu nhánh HR.
- Có refractory period 360ms để tránh đếm 2 lần cùng một nhịp.
- Tạo buffer R-R (8 phần tử) để lấy trung bình trượt.
- Chuyển đổi BPM bằng 60000 / RR_avg.
- Chặn BPM trong ngưỡng hợp lý 45-170.
- Timeout 3 giây không có peak thì reset BPM về 0.

## 6.4 Độ ổn định lúc khởi động

Code có pre-load state bộ lọc từ mẫu ADC đầu để tránh bước nhảy giả đầu vào (startup transient). Điều này giúp waveform lên ổn định nhanh hơn khi vừa mở máy.

---

## 7. Thuật toán Data Collector (chi tiết)

Từ [src/collect_mlx_data.cpp](src/collect_mlx_data.cpp).

## 7.1 Pipeline khoảng cách

- Median 3 mẫu để khử spike nhanh.
- Nội suy tuyến tính theo bảng điểm hiệu chuẩn raw -> actual.
- EMA thích nghi theo độ chênh mẫu mới:
  - Đứng yên: alpha thấp để mượt.
  - Di chuyển nhanh: alpha cao để bám nhanh.

## 7.2 Chất lượng mẫu khi đo chính thức

Mẫu hợp lệ khi đồng thời thỏa:

- Khoảng cách trong dải DIST_MIN_MM đến DIST_MAX_MM.
- Nhiệt độ môi trường trong dải AMBIENT_MIN_C đến AMBIENT_MAX_C.
- Nhiệt độ đối tượng trong dải BODY_SENSOR_MIN_C đến BODY_SENSOR_MAX_C.
- Giá trị nhiệt độ phải hữu hạn.

Nếu đang tích lũy mà gặp mẫu không hợp lệ:

- Reset ngay chuỗi đếm về 0.
- Buộc người đo phải giữ điều kiện ổn định liên tục.

## 7.3 Truyền cloud

- Endpoint là Google Script URL.
- Truyền tham số: id, obj, amb, dist.
- Cấu hình timeout mạng ngắn để phản hồi rõ:
  - Connect timeout 2s.
  - Request timeout 3s.

---

## 8. Module MAX30102 và MLX90614 (luồng test)

## 8.1 MAX30102 test

Từ [src/max30102.cpp](src/max30102.cpp):

- Sample rate: 100Hz.
- Buffer 100 mẫu, cuốn chiếu 25 mẫu/lần.
- Heart rate từ thuật toán Maxim.
- SpO2 từ thuật toán RMS tự triển khai + low-pass mượt số.
- Kiểm tra ngón tay bằng IR threshold.

## 8.2 MLX90614 test

Từ [src/mlx90614.cpp](src/mlx90614.cpp):

- Lọc trung bình 10 mẫu.
- Tách logic đo vật thể và đo người theo ngưỡng.
- Có hàm bù thân nhiệt dựa trên nhiệt độ môi trường.

---

## 9. Giao diện và điều khiển người dùng

## 9.1 Bản ECG UI

- Chart 80 điểm, update 50Hz.
- Dùng pha trộn trung bình cửa sổ + peak để vừa mượt vừa giữ R-wave.
- Có warning lớn khi mất điện cực.
- Có dot nhấp nháy theo BPM.

## 9.2 Bản Data Collector UI

- Header hiển thị trạng thái WiFi.
- Khối thông tin: ID, mẫu hiện tại, khoảng cách, nhiệt độ da, nhiệt độ môi trường.
- Footer hướng dẫn thao tác phím cứng.
- Popup đa trạng thái (đang đo, thành công, lỗi).

## 9.3 Bản Menu tổng quát

Từ [src/ui.cpp](src/ui.cpp):

- Grid menu 3x2.
- Điều hướng bằng UP/DOWN/LEFT/RIGHT.
- Xử lý long-press repeat cho điều hướng.
- Trạng thái editing cho màn password WiFi.
- Dialog xác nhận khi thoát màn hình.

---

## 10. Mapping phần cứng và GPIO

Nguồn tham chiếu chính: [GPIO_PIN_MAPPING.md](GPIO_PIN_MAPPING.md) và code hiện tại.

## 10.1 I2C

- SDA: GPIO21
- SCL: GPIO22

Thiết bị dùng chung bus:

- ADS1115
- MAX30102
- MLX90614
- VL53L0X (trong collector)

## 10.2 SPI LCD

- MOSI: GPIO23
- MISO: GPIO19
- SCK: GPIO18
- CS: GPIO15
- DC: GPIO16
- RST: GPIO17

## 10.3 ECG lead-off

- LO+: GPIO13
- LO-: GPIO14

## 10.4 Buttons

- UP: GPIO32
- DOWN: GPIO33
- LEFT: GPIO25
- RIGHT: GPIO26
- ENTER: GPIO27

Ghi chú quan trọng:

- Có các pin strapping cần lưu ý khi thiết kế PCB và mạch kéo.
- Tránh dùng GPIO6-GPIO11 vì dành cho flash.

---

## 11. Tần số cập nhật và ngân sách thời gian

Ước lượng theo mã nguồn:

- ECG core sampling: 250Hz (4ms).
- ECG chart render: 50Hz (20ms cửa sổ).
- Data collector live update: 10Hz (100ms).
- LVGL handler: chạy liên tục trong loop, delay nhỏ 5ms.

Hàm ý thiết kế:

- ECG xử lý nhanh và tách rõ nhánh detection vs nhánh hiển thị.
- UI không cần render 250Hz để tiết kiệm tài nguyên.
- Cơ chế cửa sổ + peak giúp giảm mất đỉnh khi downsample.

---

## 12. Độ tin cậy, an toàn và giới hạn hiện tại

## 12.1 Điểm mạnh

- Kiến trúc tách module rõ theo mục tiêu sử dụng.
- Có fallback và timeout cho nhiều thao tác cảm biến/mạng.
- Thuật toán ECG có nhiều lớp lọc, chú ý startup và baseline.
- Data Collector có chất lượng mẫu chặt chẽ.

## 12.2 Rủi ro cần theo dõi

- Một số tài liệu README cũ có thể chưa khớp hoàn toàn với cấu trúc code hiện tại.
- Có nhiều entry firmware khác nhau, cần quy chuẩn quy trình build theo mục đích.
- Một số luồng dùng delay trong loop/event; khi mở rộng tính năng nên cân nhắc non-blocking.
- Luồng cloud hiện dùng HTTP GET đơn giản, chưa có lớp retry/backoff nâng cao.

## 12.3 Khuyến nghị kỹ thuật

- Chuẩn hóa tài liệu chính thức theo environment thực tế đang dùng.
- Thêm unit test hoặc integration test cho ngưỡng và thuật toán trọng yếu.
- Bổ sung logging cấu trúc cho các trạng thái lỗi mạng/sensor.
- Định nghĩa profile calibration theo thiết bị thay vì hard-code duy nhất.

---

## 13. Hướng dẫn build, nạp và chạy nhanh

Có thể thao tác bằng PlatformIO theo môi trường:

1. Chọn environment tương ứng trong VS Code.
2. Build.
3. Upload.
4. Mở Serial Monitor tốc độ 115200.

Gợi ý theo mục tiêu:

- Debug ECG: dùng Main ở mode Dual trong [src/main.cpp](src/main.cpp).
- Thu dữ liệu cloud: dùng Collect_mlx_data.
- Test cảm biến riêng: dùng Max30102 hoặc Mlx90614.
- Demo menu dashboard: dùng Lcd.

---

## 14. Danh sách file trọng yếu cần nắm

### 14.1 Build và cấu hình

- [platformio.ini](platformio.ini)
- [include/lv_conf.h](include/lv_conf.h)

### 14.2 ECG core

- [src/main.cpp](src/main.cpp)
- [src/ad8232.cpp](src/ad8232.cpp)
- [include/ad8232.h](include/ad8232.h)
- [src/ui_ecg.cpp](src/ui_ecg.cpp)
- [include/ui_ecg.h](include/ui_ecg.h)

### 14.3 Data collector

- [src/collect_mlx_data.cpp](src/collect_mlx_data.cpp)
- [src/ui_datacollector.cpp](src/ui_datacollector.cpp)
- [include/ui_datacollector.h](include/ui_datacollector.h)

### 14.4 Dashboard/menu UI

- [src/lcd.cpp](src/lcd.cpp)
- [src/ui.cpp](src/ui.cpp)
- [include/ui.h](include/ui.h)
- [include/custom_icons.h](include/custom_icons.h)

### 14.5 Hỗ trợ và tài liệu phần cứng

- [GPIO_PIN_MAPPING.md](GPIO_PIN_MAPPING.md)
- [AD8232_WIRING.md](AD8232_WIRING.md)
- [ECG_USER_GUIDE.md](ECG_USER_GUIDE.md)
- [ECG_TROUBLESHOOTING_GUIDE.md](ECG_TROUBLESHOOTING_GUIDE.md)
- [UI_DATACOLLECTOR_README.md](UI_DATACOLLECTOR_README.md)

---

## 15. Quy trình vận hành khuyến nghị cho team

1. Xác định đúng mục tiêu chạy firmware trước khi build.
2. Chọn environment đúng trong [platformio.ini](platformio.ini).
3. Kiểm tra dây và nguồn theo [GPIO_PIN_MAPPING.md](GPIO_PIN_MAPPING.md).
4. Chạy smoke test sensor trước khi demo chính thức.
5. Chạy monitor log và lưu log cho mỗi phiên test.
6. Nếu sửa thuật toán, kiểm định lại bằng cùng kịch bản test chuẩn.

---

## 16. Checklist trước khi đóng gói release

- Build sạch thành công cho environment mục tiêu.
- Upload ổn định trên board thật.
- Không có lỗi giao tiếp sensor kéo dài.
- UI không treo khi thao tác liên tục bằng phím cứng.
- Dữ liệu cloud gửi thành công nhiều lần liên tiếp.
- Kiểm tra mất mạng, mất sensor, mất lead và hồi phục.
- Cập nhật tài liệu thay đổi ở file này sau mỗi đợt chỉnh lớn.

---

## 17. Kết luận

Firmware hiện tại đã đạt mức hoàn chỉnh tốt cho cả:

- Theo dõi ECG thời gian thực.
- Thu thập dữ liệu nhiệt độ và khoảng cách có kiểm soát chất lượng mẫu.
- Vận hành UI đa dạng theo nhiều use-case.

Điểm trọng tâm giai đoạn tiếp theo nên là chuẩn hóa tài liệu và quy trình test/release để giảm sai lệch giữa các luồng firmware.

---

## 18. Phụ lục: tài liệu này liên kết với file tổng hợp code

Nếu cần đọc toàn bộ code trong một file duy nhất, dùng:

- [FIRMWARE_ALL_IN_ONE.txt](FIRMWARE_ALL_IN_ONE.txt)

Nếu cần tài liệu tóm lược kỹ thuật cực chi tiết, dùng chính file hiện tại:

- [FIRMWARE_TONG_HOP_CHI_TIET.md](FIRMWARE_TONG_HOP_CHI_TIET.md)
