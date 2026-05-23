# Phân tích firmware cho environment Lcd (PlatformIO)

Tài liệu này mô tả chi tiết firmware của môi trường Lcd, bám sát cấu trúc build, luồng runtime, UI, các module cảm biến, kết nối mạng, MQTT và logic thu thập dữ liệu. Nội dung dựa trên các file nguồn chính: [platformio.ini](platformio.ini), [src/lcd.cpp](src/lcd.cpp), [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp), [src/ui.cpp](src/ui.cpp), [src/ui_datacollector.cpp](src/ui_datacollector.cpp), [src/boot_screen.cpp](src/boot_screen.cpp), [src/ad8232.cpp](src/ad8232.cpp), các header và module cảm biến trong thư mục include/src.

---

## 1) Cấu hình build và phạm vi file được compile

**Environment Lcd** được định nghĩa trong [platformio.ini](platformio.ini):

- `build_src_filter` chỉ biên dịch đúng các file cần cho LCD flow:
  - Core flow: [src/lcd.cpp](src/lcd.cpp)
  - Runtime sensors: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp)
  - UI presenter/status: [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp)
  - UI chính + collector UI: [src/ui.cpp](src/ui.cpp), [src/ui_datacollector.cpp](src/ui_datacollector.cpp)
  - Sensor modules: [src/max30102_sensor_module.cpp](src/max30102_sensor_module.cpp), [src/mlx90614_sensor_module.cpp](src/mlx90614_sensor_module.cpp), [src/ad8232_sensor_module.cpp](src/ad8232_sensor_module.cpp), [src/ina219_sensor_module.cpp](src/ina219_sensor_module.cpp)
  - ECG backend: [src/ad8232.cpp](src/ad8232.cpp)
  - WiFi config & boot: [src/wifi_config_manager.cpp](src/wifi_config_manager.cpp), [src/boot_screen.cpp](src/boot_screen.cpp)
  - Icons LVGL: [src/icons/\*.c](src/icons)
- `board_build.partitions = huge_app.csv` để đủ flash cho LVGL/UI lớn.
- LVGL + TFT_eSPI được add riêng cho env này.
- Build flags cấu hình TFT ILI9341 + LVGL include path + tăng loop stack 32KB.

### 1.1 Thư viện và build flags chi tiết

- **Global lib_deps** (dùng chung):
  - MAX30100_milan, MAX3010x SparkFun
  - Adafruit GFX + ILI9341
  - Adafruit MLX90614, ADS1X15, VL53L0X, INA219
  - PubSubClient, ArduinoJson
- **Env Lcd lib_deps**: LVGL v8.3.9, TFT_eSPI.
- **Build flags chính**:
  - ILI9341 pins: MISO=19, MOSI=23, SCLK=18, CS=15, DC=16, RST=17
  - SPI_FREQUENCY=27000000
  - LVGL include flags: `LV_CONF_INCLUDE_SIMPLE`, `LV_COMP_CONF_INCLUDE_SIMPLE`, `LV_LVGL_H_INCLUDE_SIMPLE`
  - Include path `-I include` để lấy [include/lv_conf.h](include/lv_conf.h)
  - Loop stack: `CONFIG_ARDUINO_LOOP_STACK_SIZE=32768`

**Kết luận:** env Lcd là một firmware tích hợp UI LCD + nhiều sensor + WiFi/MQTT, với entry point chính là [src/lcd.cpp](src/lcd.cpp).

---

## 2) Tổng quan kiến trúc firmware Lcd

**Ba lớp chính**:

1. **Flow Coordinator**: [src/lcd.cpp](src/lcd.cpp)

- Khởi tạo I2C, tasks, LVGL UI, WiFi, MQTT, vòng lặp logic runtime.
- Chia việc sampling sensor (task riêng) và cập nhật UI (guiTask).

2. **Sensor Runtime Orchestrator**: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [include/lcd_sensor_runtime.h](include/lcd_sensor_runtime.h)

- Lớp gom module cảm biến: MAX30102, MLX90614, AD8232, INA219.
- Cung cấp snapshot và trạng thái sẵn sàng cho UI.

3. **UI Layer**: [src/ui.cpp](src/ui.cpp), [src/ui_datacollector.cpp](src/ui_datacollector.cpp), [src/boot_screen.cpp](src/boot_screen.cpp), [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp)

- UI menu + các màn hình đo (ECG, SpO2, Temp, Monitor, MeasureAll).
- Boot screen hiển thị trạng thái cảm biến.
- UI presenter cập nhật status text/màu theo trạng thái sensor, WiFi.

---

## 3) Luồng khởi động (Setup) và task runtime

### 3.1 setup() trong [src/lcd.cpp](src/lcd.cpp)

- `Serial.begin(115200)` + log reset reason/chip rev.
- Hàm constructor `early_boot_log()` gọi `ets_printf("[ROM] app_start")` trước setup.
- Khởi tạo I2C bus 0 (SDA=21, SCL=22) ở 50kHz.
- I2C scanner **full-range 1..126** cho bus 0 và bus 1 (Wire1 với SDA=4, SCL=5).
- Tạo mutex `i2c0Mutex` để serialize truy cập I2C bus 0.
- Tạo task LVGL/GUI: `xTaskCreatePinnedToCore(guiTask, ...)` chạy trên core 1.

### 3.2 guiTask() - runtime chính

Chuỗi khởi tạo:

1. **Tạo task sampler**:
   - `ecgSamplingTask` (core 0, prio 3) đọc ECG ADS1115 ở 250Hz khi cần.
   - `maxSamplingTask` (core 0, prio 2) đọc MAX30102 ở 100Hz khi cần.
2. `ui_init()` tạo LVGL, render boot screen.
3. `wifiConfigManager.begin()` tải cấu hình WiFi/MQTT.
4. `sensorRuntime.begin()` init MLX90614, AD8232, INA219.
5. Probe MAX30102 và VL53L0X để cập nhật boot badges.
6. Cấu hình MQTT (device_id, topic, host/port).
7. Chờ 3s và chuyển từ boot sang menu.
8. Vòng lặp chính (while 1):
   - `lv_timer_handler()`
   - `wifiConfigManager.update()`
   - `mqttClient.loop()`
   - `sensorRuntime.updateBackground()` nếu cần MLX/INA219
   - Điều khiển UI và publish telemetry theo screen.

### 3.3 ecgSamplingTask()

- Chỉ lấy ECG khi đang ở màn hình ECG hoặc MeasureAll.
- Dùng `sensorRuntime.updateEcgBackground()` mỗi 4ms (250Hz).
- Khi không cần, rơi về 30ms để giảm tải I2C/CPU.

### 3.4 maxSamplingTask()

- Chỉ chạy khi ở MONITOR/SPO2/MEASUREALL và MAX30102 ready.
- Lấy mutex I2C trước khi đọc.
- Tần số: 100Hz (delay 10ms).

### 3.5 Các hằng số thời gian và ngưỡng chính trong [src/lcd.cpp](src/lcd.cpp)

- UI/sensor cadence:
  - `SENSOR_UI_UPDATE_MS = 80`
  - `ECG_SAMPLE_UPDATE_MS = 4` (250Hz)
  - `ECG_UI_UPDATE_MS = 8`
  - `ECG_LOG_INTERVAL_MS = 120`
  - `ECG_IDLE_SAMPLE_MS = 30`
  - `CONFIG_UI_UPDATE_MS = 1000`
  - `WIFI_HEADER_UPDATE_MS = 800`
- MQTT:
  - Reconnect 3000ms, publish 700ms, ok-log 1500ms
  - Payload buffer 384 bytes
- Collect/MeasureAll:
  - `COLLECT_SAMPLE_LIMIT = 5`
  - `COLLECT_MEASUREMENT_SAMPLES = 30`
  - Distance valid 38..48mm, offset -18mm
  - Ambient 20..35C, Body 32..42C
- State stabilization: `STATE_CONFIRM_TICKS = 3`

### 3.6 Biến toàn cục và đối tượng chính trong [src/lcd.cpp](src/lcd.cpp)

- LVGL/TFT: `TFT_eSPI tft` là instance thật (không chỉ extern).
- Runtime managers: `LcdSensorRuntime sensorRuntime`, `WifiConfigManager wifiConfigManager`.
- MQTT: `PubSubClient mqttClient`, `mqttDeviceId`, `mqttPublishTopic`, `mqttBrokerHost/Port`.
- Latest snapshots: `latestMaxSnapshot`, `latestEcgSample`, cờ `latestMaxLive/latestEcgLive`.
- Đồng bộ I2C: `SemaphoreHandle_t i2c0Mutex`.
- Data collection state: `collectCurrentId`, `collectSampleCount`, các biến pending send.
- MeasureAll pending values và flags.
- Các task handle: `ecgSamplingTaskHandle`, `maxSamplingTaskHandle`.

---

## 4) I2C topology và bus usage

**Bus 0 (Wire)**: SDA=21, SCL=22, 50kHz mặc định.

- MAX30102, MLX90614, INA219, VL53L0X.
- Có mutex `i2c0Mutex` để tránh xung đột đa task.

**Bus 1 (Wire1)**: SDA=4, SCL=5, 50kHz mặc định.

- ADS1115 cho AD8232.
- Module [src/ad8232.cpp](src/ad8232.cpp) dùng Wire1 ở 400kHz khi init, và tự recover nếu lỗi.

**Lưu ý**: Lcd firmware đang chạy full I2C scan 0x01..0x7E ở setup cho cả 2 bus. Điều này hữu ích cho debug nhưng có thể làm treo trên một số board/thiết bị nhạy.

### 4.1 Tần số I2C theo module

- MAX30102 đặt `Wire.setClock(50000)` trong begin (bus 0).
- MLX90614 ép `Wire.setClock(100000)` trước mỗi lần đọc để ổn định.
- INA219 đặt `Wire.setClock(50000)` và `Wire.setTimeOut(500)`.
- VL53L0X dùng bus 0, có logic reset I2C nếu RangeStatus lỗi.
- ADS1115 (AD8232) dùng Wire1 400kHz khi init, sau đó recover nếu lỗi kéo dài.

---

## 5) Sensor Runtime Orchestrator (LcdSensorRuntime)

### 5.1 Lớp LcdSensorRuntime

File: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [include/lcd_sensor_runtime.h](include/lcd_sensor_runtime.h)

- **begin()**
  - Init MLX90614 (retry nếu fail).
  - Init AD8232/ADS1115 một lần.
  - Init INA219 (đặt BatteryType LiPo 2S).
  - Snapshot MLX để sync trạng thái boot screen.

- **updateBackground(enableMlxUpdate)**
  - Luôn update INA219.
  - Chỉ update MLX khi enable.
  - Cập nhật `mlxReady` và `mlxConnected`.

- **updateEcgBackground(enableEcgUpdate)**
  - Khi ECG active, gọi `ad8232Module_.update()`.
  - Snapshot ECG được lưu với `portENTER_CRITICAL`.

- **beginMax30102() / updateMax30102()**
  - Start MAX30102, update snapshot với lock.

- **Snapshot getters**
  - `maxSnapshot()`, `ecgSnapshot()` đọc an toàn qua mux.

### 5.1.1 Cấu trúc dữ liệu snapshot

- `SensorSnapshot` (xem [include/sensor_module.h](include/sensor_module.h)):
  - `bodyTempC`, `heartRateBpm`, `spo2Percent`, `waveform`
  - `sensorReady`, `signalReady`
- `INA219Snapshot` (xem [include/ina219_sensor_module.h](include/ina219_sensor_module.h)):
  - `busVoltageV`, `currentMa`, `powerMw`, `shuntVoltageV`
  - `batteryPercent`, `isCharging`, `sensorReady`

### 5.2 AD8232 Sensor Module

File: [src/ad8232_sensor_module.cpp](src/ad8232_sensor_module.cpp), [include/ad8232_sensor_module.h](include/ad8232_sensor_module.h)

- Sampling 250Hz, catch-up tối đa 16 mẫu/loop.
- Khi leads off hoặc sensor mất: waveform = 50, HR = 0.
- Lọc hiển thị: baseline tracking, EMA smoothing, envelope scaling, soft limiter.

### 5.3 MAX30102 Sensor Module

File: [src/max30102_sensor_module.cpp](src/max30102_sensor_module.cpp), [include/max30102_sensor_module.h](include/max30102_sensor_module.h)

- I2C 50kHz, `MAX30105` driver.
- `finger threshold` = 15000 IR.
- Beat detection bằng `checkForBeat()`.
- HR: EMA 0.7/0.3, timeout 5s reset.
- SpO2: RMS ratio red/IR, EMA 0.6/0.4.
- Waveform pleth: high-pass + auto gain + normalization.

### 5.4 MLX90614 Sensor Module

File: [src/mlx90614_sensor_module.cpp](src/mlx90614_sensor_module.cpp), [include/mlx90614_sensor_module.h](include/mlx90614_sensor_module.h)

- `kReadIntervalMs = 1500`, retry 2500.
- Read raw object/ambient, validate range, handle errors.
- Nếu lỗi nhiều lần: mark not ready và chờ reconnect.

### 5.5 INA219 Sensor Module (Battery)

File: [src/ina219_sensor_module.cpp](src/ina219_sensor_module.cpp), [include/ina219_sensor_module.h](include/ina219_sensor_module.h)

- Đọc voltage/current/power mỗi 500ms.
- EMA lọc voltage, lookup table pin theo LiPo/LiFe.
- Latching % pin (không nhảy lung tung khi sạc/xả).

---

## 6) ECG Backend (ad8232.cpp)

File: [src/ad8232.cpp](src/ad8232.cpp), [include/ad8232.h](include/ad8232.h)

**Phần cứng**:

- ADS1115 đọc A0, LO+ / LO- ở GPIO13/14.
- I2C bus 1 (SDA=4, SCL=5).

**Pipeline xử lý**:

1. Read ADC (ADS1115) với debounce lỗi.
2. Validate ADC, suppress glitch, step limiter.
3. Filters:
   - Moving Average
   - Median
   - Notch 50Hz
   - High-pass
   - Low-pass thích nghi (quiet vs QRS)
4. HR detection:
   - Envelope + derivative adaptive threshold
   - refractory 430ms, HR range 45-130
   - RR buffer 8 samples, outlier gating

**API dùng trong LCD**:

- `sampleAD8232Now()` chạy ở task 250Hz.
- `getECGFilteredSignal()`, `getHeartRate()`, `areLeadsConnected()`.

---

## 7) UI Layer tổng quan

### 7.1 UI chính (ui.cpp)

File: [src/ui.cpp](src/ui.cpp), [include/ui.h](include/ui.h)

**LVGL setup**:

- Buffer: 320×20 lines.
- Input driver kiểu keypad.
- Button mapping: UP=32, DOWN=33, LEFT=25, RIGHT=26, ENTER=27.

**Screen types**:

- Boot, Menu, Monitor, ECG, SpO2, Temp, CollectData, MeasureAll, WiFi Scan, WiFi Pass, Config.

**Các thành phần UI quan trọng**:

- Header: MQTT status, battery indicator, WiFi icon/signal.
- ECG chart (main + mini), labels HR/SpO2/Temp.
- Dialog xác nhận quay về menu.
- WiFi scan list + password keyboard.

**API UI trong [include/ui.h](include/ui.h)** (được lcd.cpp gọi):

- `ui_switch_screen()`, `ui_update_sensors()`, `ui_update_ecg_live()`
- `ui_set_ambient_temp()`, `ui_set_temp_distance()`
- `ui_set_sensor_status()`, `ui_set_header_wifi()`, `ui_set_mqtt_status()`
- `ui_set_measure_all_values()`, `ui_set_measure_all_status()`
- `ui_set_battery()`
- Các API phụ trợ khác trong header: `ui_update_measure_all_ecg_waveform()`, `ui_set_measure_all_send_state()`, `ui_input_read()`.

### 7.2 Xử lý input chi tiết (ui_input_read)

Logic đọc phím trong [src/ui.cpp](src/ui.cpp):

- **Debounce 300ms** sau mỗi lần đổi màn hình để tránh trigger nhầm.
- **Long-press repeat**: delay 500ms, repeat 150ms cho UP/DOWN/LEFT/RIGHT.
- **Dialog mode**: khi dialog mở, LEFT/RIGHT chọn YES/NO, ENTER xác nhận.
- **MENU**: điều hướng dạng grid 4×2 bằng LEFT/RIGHT/UP/DOWN.
- **WIFI_PASS**:
  - Nếu đang edit: LEFT/RIGHT/UP/DOWN điều hướng keyboard.
  - Giữ LEFT 1s để thoát editing.
  - ESC quay lại WiFi scan.
- **COLLECTDATA**:
  - LEFT: ID- (giữ 1s -> back)
  - RIGHT: ID+
  - DOWN: reset
  - ENTER: đo/confirm
- **Các màn hình đo khác** (Monitor/ECG/SpO2/Temp/MeasureAll):
  - ENTER toggle MQTT send ON/OFF.
  - LEFT gửi ESC để back ngay.

### 7.2.1 Quản lý vòng đời screen

- `ui_switch_screen()` gọi các hàm `build_*` tương ứng.
- `clean_resources()` xóa timer, reset con trỏ widget để tránh stale pointer.
- `switch_to_obj()` load screen mới và `lv_obj_del()` screen cũ để tiết kiệm RAM.

### 7.3 Cơ chế giảm flicker và cache trong UI

- `ui_update_sensors()` cache giá trị HR/SpO2/Temp để chỉ update khi đổi.
- `ui_set_sensor_status()`, `ui_set_config_status()`, `ui_set_header_wifi()` lưu lastMsg/lastColor để tránh set lại liên tục.
- `ui_set_battery()` chỉ đổi icon/color khi % pin hoặc trạng thái sạc thay đổi.

### 7.3.1 Hàm update dữ liệu chuyên biệt

- `ui_update_ecg_live()` cập nhật chart ECG, HR, beat dot theo cadence 8ms.
- `ui_update_measureall_ecg()` cập nhật chart mini cho MeasureAll.
- `ui_set_ambient_temp()` hiển thị ambient theo giá trị MLX.
- `ui_set_temp_distance()` hiển thị khoảng cách VL53L0X.

### 7.4 LVGL config (lv_conf.h)

Các điểm chính trong [include/lv_conf.h](include/lv_conf.h):

- Color depth 16-bit (RGB565), no swap.
- Custom allocator: `malloc/free`.
- Tick source: `millis()` (LV_TICK_CUSTOM = 1).
- Font Montserrat 8..48 đa số bật, sử dụng trong boot/menu/labels.
- LVGL logs disabled (LV_USE_LOG = 0).

### 7.5 Boot screen

File: [src/boot_screen.cpp](src/boot_screen.cpp), [include/boot_screen.h](include/boot_screen.h)

- UI bố cục 320×240, grid 2×2 sensors + TOF bar.
- Status badge cho AD8232, MAX30102, MLX90614, WiFi, VL53L0X.
- Hiển thị device id + heap.

### 7.6 UI Data Collector

File: [src/ui_datacollector.cpp](src/ui_datacollector.cpp), [include/ui_datacollector.h](include/ui_datacollector.h)

- Screen riêng cho thu thập dataset (distance + temp + ambient).
- Có popup progress, status, và hướng dẫn nút bấm.

### 7.7 UI Presenter

File: [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp)

- Chuẩn hóa update status text/màu cho WiFi, Config, sensor status.
- Tách logic hiển thị trạng thái khỏi main loop.

---

## 8) Chi tiết từng màn hình và logic cập nhật

### 8.1 Boot Screen

- `build_boot()` render UI trước khi init sensors.
- `boot_screen_set_sensor_status()` cập nhật trạng thái OK/RETRY/FAIL.
- `boot_screen_set_progress()` update tiến trình.
- `boot_screen_set_wifi_detail()` hiển thị IP/SSID trên card WiFi.
- `boot_screen_refresh_heap()` cập nhật heap ở info bar.

### 8.2 Menu

- Grid 4×2 icon: Monitor, ECG, HR/SpO2, Temp, Collect, MeasureAll, Wifi, Config.
- Điều hướng phím: custom grid navigation.

### 8.3 Monitor

- Màn hình tổng hợp: HR, SpO2, Body Temp, Ambient Temp.
- Status line dưới cùng hiển thị tình trạng sensor.

### 8.4 ECG

- Chart 160 điểm, cập nhật ở 8ms.
- Hiển thị HR + beat dot, status lead-off.

### 8.5 SpO2

- HR + SpO2 big numbers, chart pleth 80 điểm.
- Status hiển thị sensor state.

### 8.6 Temp

- Body temp trung tâm, ambient temp, distance (VL53L0X).
- Status hiển thị MLX state.

### 8.7 CollectData

- UI riêng cho dataset collection.
- Enter: đo và gửi; Left/Right: ID -/+; Down: reset.
- Popup hướng dẫn confirm gửi.

### 8.8 MeasureAll

- Hiển thị HR, SpO2, Temp, ECG waveform, Distance.
- ENTER: chuẩn bị & gửi 1 gói dữ liệu.

### 8.9 WiFi Scan/Pass

- Scan async, list SSID, chọn và nhập password.
- Keyboard LVGL cho nhập pass.

### 8.10 Config

- Hiển thị IP web config, trạng thái, instruction.
- Khi vào screen: force AP mode và start web server.

### 8.11 Logic khi đổi màn hình (trong guiTask)

Trong [src/lcd.cpp](src/lcd.cpp), mỗi lần `current_screen_type` đổi:

- Reset trạng thái MQTT send, cập nhật header.
- Nếu rời Config: gọi `wifiConfigManager.exitConfigMode()` (reconnect async).
- Nếu rời Collect: reset pending, cleanup UI datacollector.
- Nếu rời MeasureAll: hủy pending send.
- Khi vào MONITOR/SPO2: probe MAX30102, update status.
- Khi vào ECG: init AD8232, reset ECG UI.
- Khi vào CONFIG: force AP mode và refresh config UI.
- Khi vào COLLECTDATA: ensure VL53L0X ready, update ID/progress.
- Khi vào MEASUREALL: init MAX30102, set status label.

### 8.12 UI Presenter: trạng thái sensor/WiFi

Trong [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp):

- `refresh_config_ui()` hiển thị IP, status, instruction theo WiFi state.
- `refresh_wifi_header_ui()` hiển thị SSID + signal bars + màu.
- `notify_max_state()` và `notify_ecg_state()` chuẩn hóa text/màu status cho UI.

### 8.13 Cầu nối UI -> runtime (flags consume)

Các hàm trong [src/ui.cpp](src/ui.cpp) trả cờ để loop xử lý:

- `ui_consume_wifi_connect_request(ssid, pass)`
- `ui_consume_mqtt_send_toggle_request()`
- `ui_consume_collect_take_request()` / `ui_consume_collect_id_minus_request()` / `ui_consume_collect_id_plus_request()` / `ui_consume_collect_reset_request()`
- `ui_consume_measure_all_start_request()`

Các cờ này được set trong xử lý phím và được đọc ở `guiTask`.

---

## 9) MQTT pipeline và telemetry

File chính: [src/lcd.cpp](src/lcd.cpp), defaults: [include/mqtt_defaults.h](include/mqtt_defaults.h)

- Device ID: `ESP32_XXXXXX` từ eFuse.
- Topic: `vitals/<device_id>/data`.
- MQTT send toggle bằng nút ENTER trên screen đo.
- Publish interval: 700ms.
- Payload JSON có `device_id`, `mode`, `ts` và data tương ứng.

### 9.1 Mode publish

- `temp`: {temp}
- `ecg`: {ecg}
- `spo2`: {hr, spo2}
- `monitor`: {hr, spo2, temp}
- `measureall`: {hr, spo2, temp, ecg}

### 9.2 Reconnect logic

- Mỗi 3s thử reconnect.
- Nếu dùng default MQTT, tự lấy gateway LAN làm host.
- MQTT config lấy từ web config hoặc defaults.

### 9.3 Điều kiện publish và toggle SEND

- ENTER trên các màn hình đo sẽ toggle `mqttSendEnabled`.
- `publishTelemetryIfReady()` chỉ chạy khi:
  - SEND ON
  - Màn hình thuộc nhóm publish (Monitor/ECG/SpO2/Temp/MeasureAll)
  - Có dữ liệu hợp lệ theo từng mode
- Throttle: không publish nếu chưa đủ `MQTT_PUBLISH_INTERVAL_MS`.
- `refresh_mqtt_ui_status()` cập nhật header "SEND ON/OFF" theo state.

### 9.4 Publish cho Collect/MeasureAll

- `publishCollectData()` gửi payload riêng cho collect (dist/temp/ambient/id/sample).
- `publishMeasureAllData()` gửi 1 gói tổng hợp dist/temp/ambient/hr/spo2/ecg.

---

## 10) WiFi Config Manager

File: [src/wifi_config_manager.cpp](src/wifi_config_manager.cpp), [include/wifi_config_manager.h](include/wifi_config_manager.h)

- Lưu SSID, pass, email, MQTT config bằng Preferences.
- AP mode SSID: `ESP32-xxxxxx`, pass `12345678`.
- Web server endpoints:
  - `/` HTML UI
  - `/scan` trả JSON list SSID
  - `/config` trả JSON cấu hình
  - `/save` lưu và connect

- Khi rời config screen: reconnect STA theo async state machine.

### 10.2 Web UI (HTML) cấu hình

- HTML nằm trong [include/web_interface.h](include/web_interface.h) (PROGMEM).
- Form gồm: Email, WiFi SSID/Pass, MQTT custom toggle + host/port/user/pass.
- Có API `fillDefaultMqtt()` để reset MQTT mặc định.

### 10.1 Các khóa Preferences và logic reconnect

- Keys lưu trong namespace `wifi_cfg`:
  - `ssid`, `pass`, `email`
  - `mqtt_custom`, `mqtt_host`, `mqtt_port`, `mqtt_user`, `mqtt_pass`
- `exitConfigMode()`:
  - Tắt AP, nếu có saved SSID thì reconnect async (10s).
  - Nếu timeout: giữ OFFLINE, tránh block UI.
- `forceStartAp()`:
  - Bật AP + start web server khi vào screen Config.

---

## 11) VL53L0X (Distance) và Data Collector pipeline

Trong [src/lcd.cpp](src/lcd.cpp):

- `collect_ensure_lox_ready()` probe sensor.
- `collect_live_distance()` đọc nhanh, median + EMA + calibration.
- Calibration bằng bảng 12 điểm raw/actual.
- Measurement logic:
  - Lấy 30 mẫu (valid temp + distance), tính mean.
  - Offset phần cứng: -18mm.
  - Popup confirm rồi gửi.

### 11.1 Lọc và hiệu chỉnh khoảng cách

- Median 3 mẫu đầu vào (`collect_median3`).
- Interpolate theo bảng `collectRawPts` / `collectActualPts` (12 điểm).
- EMA động: alpha tăng khi delta lớn để bám nhanh.
- Nếu VL53L0X trả RangeStatus lỗi, gọi `collect_recover_i2c()` để reset bus.

### 11.2 Trạng thái pending send và confirm

- `collectPendingSend` giữ kết quả đo chờ người dùng ENTER gửi.
- DOWN hủy pending.
- Nếu WiFi mất: popup báo lỗi, vẫn giữ pending để thử lại.

### 11.3 HTTP Google Script

- Gửi bằng `WiFiClientSecure` (setInsecure) + `HTTPClient`.
- Tham số URL: `id`, `obj`, `amb`, `dist`.
- Timeout connect 3s, read 4s, follow redirect.

**Điều kiện hợp lệ**:

- Distance: 38..48mm
- Ambient: 20..35°C
- Body: 32..42°C

**Gửi dữ liệu**:

- Gửi MQTT hoặc HTTP tới Google Script URL.

---

## 12) Các tham số thời gian quan trọng

Trong [src/lcd.cpp](src/lcd.cpp):

- Sensor UI update: 80ms.
- ECG sample: 4ms (250Hz).
- ECG UI frame: 8ms.
- ECG log interval: 120ms.
- Config UI update: 1000ms.
- WiFi header update: 800ms.
- MQTT publish interval: 700ms.

### 12.1 Battery refresh cadence

- GUI loop update pin mỗi 2s bằng `ui_set_battery()`.
- Log battery status định kỳ với điện áp và dòng đo từ INA219.
- MQTT reconnect: 3000ms.

---

## 13) Cơ chế ổn định trạng thái (StateTracker)

- `STATE_CONFIRM_TICKS = 3`.
- Áp dụng cho MaxRuntimeState và EcgRuntimeState.
- Trạng thái chỉ thay đổi khi xác nhận đủ 3 lần liên tiếp.
- Giảm flicker status trên UI.

Các enum (xem [include/lcd_ui_presenter.h](include/lcd_ui_presenter.h)):

- `MaxRuntimeState`: NoSensor, WaitingFinger, LiveData.
- `EcgRuntimeState`: NoSensor, LeadsOff, LiveData.

---

## 14) Các luồng dữ liệu chính

### 14.1 MAX30102 → UI

MAX30102 module đọc IR/RED → tính HR/SpO2 → snapshot → UI update (Monitor/SPO2/MeasureAll).

### 14.2 MLX90614 → UI

MLX module đọc object/ambient → snapshot → UI Temp/Monitor/MeasureAll.

### 14.3 AD8232 → ECG

ADS1115 đọc ECG → filter → HR detect → UI ECG chart + BPM.

### 14.4 INA219 → Header Battery

INA219 snapshot → ui_set_battery() cập nhật icon + %.

---

## 15) Logging/Diagnostics

- Boot log hiển thị reset reason, chip revision.
- I2C scanner hiển thị thiết bị phát hiện.
- Log status sensor và MQTT publish.
- ECG raw/filtered log mỗi 120ms khi live.

---

## 16) File map (tóm tắt vai trò)

- Core flow: [src/lcd.cpp](src/lcd.cpp)
- Runtime sensors: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [include/lcd_sensor_runtime.h](include/lcd_sensor_runtime.h)
- UI presenter: [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp), [include/lcd_ui_presenter.h](include/lcd_ui_presenter.h)
- UI screens: [src/ui.cpp](src/ui.cpp), [include/ui.h](include/ui.h)
- Data collector UI: [src/ui_datacollector.cpp](src/ui_datacollector.cpp), [include/ui_datacollector.h](include/ui_datacollector.h)
- Boot screen: [src/boot_screen.cpp](src/boot_screen.cpp), [include/boot_screen.h](include/boot_screen.h)
- AD8232 backend: [src/ad8232.cpp](src/ad8232.cpp), [include/ad8232.h](include/ad8232.h)
- Sensor modules: [src/max30102_sensor_module.cpp](src/max30102_sensor_module.cpp), [src/mlx90614_sensor_module.cpp](src/mlx90614_sensor_module.cpp), [src/ad8232_sensor_module.cpp](src/ad8232_sensor_module.cpp), [src/ina219_sensor_module.cpp](src/ina219_sensor_module.cpp)
- WiFi config: [src/wifi_config_manager.cpp](src/wifi_config_manager.cpp), [include/wifi_config_manager.h](include/wifi_config_manager.h)
- MQTT defaults: [include/mqtt_defaults.h](include/mqtt_defaults.h)
- LVGL config: [include/lv_conf.h](include/lv_conf.h)

---

## 17) Kết luận

Environment Lcd là firmware “all-in-one” với: UI LVGL chuyên dụng, nhiều task sampling, cảm biến đa dạng (ECG/SpO2/Temp/Distance/Battery), WiFi + Web Config, MQTT publish, và workflow đo/collect dữ liệu. Luồng runtime phân tách rõ: `lcd.cpp` điều phối, `lcd_sensor_runtime` gom sensor, `ui.cpp` hiển thị và `wifi_config_manager` quản lý kết nối.
