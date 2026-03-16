# 🏥 HỆ THỐNG GIÁM SÁT SỨC KHỎE Y TẾ IoT

> **Đồ Án Tốt Nghiệp 2026** - ESP32 + Cảm Biến Y Tế

---

## 🎯 TÍNH NĂNG

- ✅ **Nhịp Tim (HR)** - MAX30105 qua I2C
- ✅ **Oxy Máu (SpO2)** - MAX30105 qua I2C
- ✅ **Nhiệt Độ** - MLX90614 qua I2C
- ✅ **ECG** - AD8232 + ADS1115 (16-bit ADC)
- ✅ **Hiển Thị** - ILI9341 LCD 320x240 qua SPI
- ✅ **Xử Lý Tín Hiệu** - Low-pass filter, AGC, beat detection

---

## 🔧 THIẾT BỊ

| Thiết Bị | Chức Năng           | Địa Chỉ   |
| -------- | ------------------- | --------- |
| ESP32    | Vi điều khiển chính | -         |
| MAX30105 | Nhịp tim + SpO2     | I2C: 0x57 |
| MLX90614 | Nhiệt độ hồng ngoại | I2C: 0x5A |
| AD8232   | Cảm biến ECG chính  | Analog    |
| ADS1115  | ADC 16-bit cho ECG  | I2C: 0x48 |
| ILI9341  | Màn hình LCD        | SPI       |

---

## 📐 KẾT NỐI

### I2C Bus (GPIO21=SDA, GPIO22=SCL)

```
MAX30105: SDA→21, SCL→22, VCC→3.3V, GND→GND
MLX90614: SDA→21, SCL→22, VCC→3.3V, GND→GND
ADS1115:  SDA→21, SCL→22, VCC→5V,   GND→GND
```

### SPI Bus (LCD)

```
ILI9341: CS→15, DC→2, RST→4, MOSI→23, SCK→18, VCC→5V, GND→GND
```

### ECG Module

```
AD8232:  OUTPUT→ADS1115_A0, LO+→GPIO25, LO-→GPIO26, VCC→3.3V
         RA(đỏ)→cánh tay phải, LA(vàng)→cánh tay trái, RL(xanh)→chân phải
```

---

## 📂 CẤU TRÚC

```
ESP32/
├── platformio.ini         # Cấu hình 8 environments
├── switch_env.bat         # Script test (Windows)
├── README.md              # File này
│
├── src/                   # Code chính
│   ├── main.cpp           # Main program (tích hợp đầy đủ)
│   ├── sensors/           # Module cảm biến
│   │   ├── MAX30105_Sensor.h
│   │   ├── MLX90614_Sensor.h
│   │   └── ADS1115_Sensor.h   # ECG ADC
│   ├── display/
│   │   └── DisplayManager.h   # UI LCD
│   ├── utils/
│   │   └── SignalProcessor.h  # Lọc tín hiệu
│   └── network/           # WiFi/MQTT (tương lai)
│
├── test_modules/          # Test riêng từng thiết bị
│   ├── 01_test_MAX30105.cpp
│   ├── 02_test_MLX90614.cpp
│   ├── 03_test_ILI9341.cpp
│   ├── 04_test_ALL_SENSORS.cpp
│   └── 05_test_ECG.cpp
│
└── include/               # Headers chung
    ├── heartRate.h
    └── signal_utils.h
```

---

## 🚀 SỬ DỤNG

### **CÁCH 1: Script (Dễ nhất)**

```bash
# Double-click
switch_env.bat

# Chọn số:
[1] main - Hệ thống đầy đủ
[2] test_max30105 - Test HR/SpO2
[3] test_mlx90614 - Test nhiệt độ
[4] test_lcd - Test màn hình
[5] test_all_sensors - Test I2C
[6] test_ecg - Test ECG
[7] main_debug - Debug mode
[8] main_ota - OTA upload
```

### **CÁCH 2: VS Code**

```
1. Click thanh status bar dưới
2. Chọn environment (main, test_max30105, v.v.)
3. Click Upload (→)
4. Mở Serial Monitor (Ctrl+Shift+P → PlatformIO: Monitor)
```

### **CÁCH 3: Terminal**

```bash
# Test từng thiết bị
pio run -e test_max30105 --target upload
pio run -e test_mlx90614 --target upload
pio run -e test_ecg --target upload

# Test tổng hợp
pio run -e test_all_sensors --target upload

# Hệ thống đầy đủ
pio run -e main --target upload

# Serial Monitor (115200 baud)
pio device monitor
```

---

## 🧪 QUY TRÌNH TEST

```
Bước 1: Test từng thiết bị riêng
├─ test_max30105    → Kiểm tra HR, SpO2, IR signal
├─ test_mlx90614    → Kiểm tra nhiệt độ
├─ test_lcd         → Kiểm tra hiển thị UI
└─ test_ecg         → Kiểm tra ECG waveform

Bước 2: Test tích hợp
└─ test_all_sensors → Kiểm tra I2C bus (0x57, 0x5A, 0x48)

Bước 3: Chạy hệ thống
└─ main             → Tích hợp tất cả modules
```

---

## ⚙️ ENVIRONMENTS

| Environment        | Mô Tả               | Command                       |
| ------------------ | ------------------- | ----------------------------- |
| `main`             | Hệ thống đầy đủ     | `pio run -e main`             |
| `test_max30105`    | Test MAX30105       | `pio run -e test_max30105`    |
| `test_mlx90614`    | Test MLX90614       | `pio run -e test_mlx90614`    |
| `test_lcd`         | Test ILI9341        | `pio run -e test_lcd`         |
| `test_all_sensors` | Test I2C sensors    | `pio run -e test_all_sensors` |
| `test_ecg`         | Test AD8232+ADS1115 | `pio run -e test_ecg`         |
| `main_debug`       | Debug với symbols   | `pio run -e main_debug`       |
| `main_ota`         | Upload qua WiFi     | `pio run -e main_ota`         |

---

## ⚠️ LƯU Ý

### Điện áp

- MAX30105, MLX90614, AD8232: **3.3V** (không dùng 5V)
- ADS1115, ILI9341: 3.3V hoặc 5V đều được

### Upload

- Nhấn nút **BOOT** trên ESP32 khi upload nếu lỗi
- Baud rate: **115200**

### I2C Scanner

```bash
# Kiểm tra địa chỉ I2C
pio run -e test_all_sensors --target upload

# Kết quả mong đợi:
# 0x48: ADS1115 (ECG ADC)
# 0x57: MAX30105 (HR/SpO2)
# 0x5A: MLX90614 (Temp)
```

### Troubleshooting

```bash
# Clean cache
pio run --target clean

# Debug
pio run -e main_debug --target upload
pio device monitor --filter esp32_exception_decoder
```

---

## 📚 THƯ VIỆN

```ini
sparkfun/SparkFun MAX3010x @ ^1.1.2
adafruit/Adafruit MLX90614 @ ^2.1.5
adafruit/Adafruit ADS1X15 @ ^2.6.2
adafruit/Adafruit GFX @ ^1.11.5
adafruit/Adafruit ILI9341 @ ^1.5.12
```

---

## 🎓 THÔNG TIN

- **Đồ án**: Hệ Thống Giám Sát Sức Khỏe Y Tế IoT
- **Năm**: 2026
- **Platform**: ESP32 + Arduino + PlatformIO

---

**✨ Chúc thành công! ✨**

---

## 📋 MỤC LỤC

- [Tính Năng](#-tính-năng)
- [Thiết Bị & Linh Kiện](#-thiết-bị--linh-kiện)
- [Sơ Đồ Kết Nối](#-sơ-đồ-kết-nối)
- [Cấu Trúc Dự Án](#-cấu-trúc-dự-án)
- [Cài Đặt & Sử Dụng](#-cài-đặt--sử-dụng)
- [Environments](#-environments)
- [Workflow Test](#-workflow-test)

---

## 🎯 TÍNH NĂNG

✅ **Đo Nhịp Tim (Heart Rate)** - MAX30105  
✅ **Đo Nồng Độ Oxy Máu (SpO2)** - MAX30105  
✅ **Đo Nhiệt Độ Không Tiếp Xúc** - MLX90614  
✅ **Hiển Thị LCD Real-time** - ILI9341 320x240  
✅ **Lọc Nhiễu Tín Hiệu** - Moving Average, Low Pass Filter, AGC  
✅ **Phát Hiện Bất Thường** - Anomaly Detection  
✅ **Kiến Trúc Module** - Dễ bảo trì và mở rộng

---

- Menu chính với 4 tùy chọn:
  - `DO SINH HIEU`: Đo và hiển thị các chỉ số sống (HR, SpO2, Temp)
  - `DO ECG`: Hiển thị sóng điện tim thời gian thực
  - `WIFI SCAN`: Quét và kết nối WiFi mới
  - `WEB CONFIG`: Cấu hình qua giao diện Web

### 2. **Đo sinh hiệu (Vital Signs)**

- **Nhịp tim**: 40-200 BPM với thuật toán phát hiện đỉnh sóng PPG
- **SpO2**: 85-100% với độ chính xác cao nhờ thuật toán AC/DC ratio
- **Nhiệt độ**: Đo không tiếp xúc từ 30-42°C
- Hiển thị dạng thẻ màu (cards) với biểu đồ sóng PPG động

### 3. **Đo ECG (Electrocardiogram)**

- Tốc độ lấy mẫu: 125Hz (8ms/sample)
- Độ phân giải: 16-bit ADC (ADS1115)

## 🔧 THIẾT BỊ & LINH KIỆN

| Thiết Bị           | Mô Tả                        | Giao Tiếp  |
| ------------------ | ---------------------------- | ---------- |
| **ESP32 DevBoard** | Vi điều khiển chính          | -          |
| **MAX30105**       | Cảm biến nhịp tim & SpO2     | I2C (0x57) |
| **MLX90614**       | Cảm biến nhiệt độ hồng ngoại | I2C (0x5A) |
| **ILI9341**        | Màn hình TFT LCD 320x240     | SPI        |

### Thư viện sử dụng:

```ini
sparkfun/SparkFun MAX3010x @ ^1.1.2
adafruit/Adafruit MLX90614 @ ^2.1.5
adafruit/Adafruit GFX @ ^1.11.5
adafruit/Adafruit ILI9341 @ ^1.5.12
```

---

## 📐 SƠ ĐỒ KẾT NỐI

### I2C Bus (MAX30105 + MLX90614):

```
ESP32          MAX30105       MLX90614
GPIO21 (SDA) → SDA         → SDA
GPIO22 (SCL) → SCL         → SCL
3.3V         → VCC         → VCC
GND          → GND         → GND
```

### SPI Bus (ILI9341 LCD):

```
ESP32          ILI9341
GPIO15       → CS
GPIO2        → DC
GPIO4        → RST
GPIO23       → MOSI
GPIO18       → SCK
5V/3.3V      → VCC
GND          → GND
```

---

## 📂 CẤU TRÚC DỰ ÁN

```
ESP32/
├── platformio.ini          # Cấu hình PlatformIO với 7 environments
├── switch_env.bat          # Script chuyển đổi môi trường (Windows)
├── README.md               # File này
│
├── src/                    # Code chính (hệ thống tích hợp)
│   ├── main.cpp            # Main program - tích hợp tất cả modules
│   │
│   ├── sensors/            # Các module cảm biến
│   │   ├── MAX30105_Sensor.h    # Wrapper cho MAX30105
│   │   └── MLX90614_Sensor.h    # Wrapper cho MLX90614
│   │
│   ├── display/            # Module quản lý màn hình
│   │   └── DisplayManager.h     # UI y tế cho ILI9341
│   │
│   ├── utils/              # Tiện ích xử lý
│   │   └── SignalProcessor.h    # Lọc nhiễu & xử lý tín hiệu
│   │
│   └── network/            # Module mạng (Giai đoạn 2)
│
├── test_modules/           # Code test riêng từng thiết bị
│   ├── 01_test_MAX30105.cpp     # Test riêng MAX30105
│   ├── 02_test_MLX90614.cpp     # Test riêng MLX90614
│   ├── 03_test_ILI9341.cpp      # Test riêng LCD
│   └── 04_test_ALL_SENSORS.cpp  # Test tổng hợp I2C sensors
│
└── include/                # Header files chung
    ├── signal_utils.h      # Thuật toán xử lý tín hiệu
    └── web_interface.h     # Web UI (Giai đoạn 2)
```

---

## 🚀 CÀI ĐẶT & SỬ DỤNG

### 1. Cài đặt PlatformIO

```bash
# Cài PlatformIO CLI
pip install platformio

# Hoặc cài extension trong VS Code
# Tìm "PlatformIO IDE" trong Extensions
```

### 2. Clone & Build

```bash
# Clone project (hoặc mở folder hiện tại)
cd E:\PlatformIO\ESP32

# Build environment mặc định (main)
pio run

# Upload lên ESP32
pio run --target upload

# Mở Serial Monitor
pio device monitor --baud 115200
```

### 3. Sử dụng Switch Script (Windows)

```bash
# Double-click file switch_env.bat
# Chọn môi trường test muốn dùng
```

---

## 🔄 ENVIRONMENTS

PlatformIO hỗ trợ **7 environments** để test riêng từng module:

| Environment        | Mô Tả                               | Build Command                 |
| ------------------ | ----------------------------------- | ----------------------------- |
| `main`             | Hệ thống tích hợp đầy đủ (mặc định) | `pio run -e main`             |
| `test_max30105`    | Test riêng MAX30105                 | `pio run -e test_max30105`    |
| `test_mlx90614`    | Test riêng MLX90614                 | `pio run -e test_mlx90614`    |
| `test_lcd`         | Test riêng ILI9341 LCD              | `pio run -e test_lcd`         |
| `test_all_sensors` | Test MAX30105 + MLX90614            | `pio run -e test_all_sensors` |
| `main_debug`       | Main với debug symbols              | `pio run -e main_debug`       |
| `main_ota`         | Upload qua WiFi (OTA)               | `pio run -e main_ota`         |

### Chuyển đổi environment trong VS Code:

1. Click vào thanh status bar dưới cùng
2. Chọn "Switch PlatformIO Project Environment"
3. Chọn environment muốn dùng

---

## 🧪 WORKFLOW TEST

### Quy trình test từ đơn giản đến phức tạp:

#### **Bước 1: Test từng thiết bị riêng**

```bash
# Test MAX30105 (Heart Rate + SpO2)
pio run -e test_max30105 --target upload
pio device monitor

# Test MLX90614 (Temperature)
pio run -e test_mlx90614 --target upload
pio device monitor

# Test ILI9341 (LCD Display)
pio run -e test_lcd --target upload
```

#### **Bước 2: Test tổng hợp I2C sensors**

```bash
# Test MAX30105 + MLX90614 cùng lúc
pio run -e test_all_sensors --target upload
pio device monitor
```

#### **Bước 3: Test hệ thống đầy đủ**

```bash
# Main program với tất cả modules
pio run -e main --target upload
pio device monitor
```

---

## 📊 GIÁM SÁT VÀ DEBUG

### Serial Monitor

```bash
# Mở Serial Monitor với baud rate 115200
pio device monitor --baud 115200 --filter colorize
```

### Debug Environment

```bash
# Build với debug symbols
pio run -e main_debug --target upload

# Xem exception decoder
pio device monitor --filter esp32_exception_decoder
```

---

## ⚠️ LƯU Ý QUAN TRỌNG

1. **Điện áp**: MAX30105 và MLX90614 dùng **3.3V** (không dùng 5V)
2. **I2C Address**: Kiểm tra bằng I2C Scanner nếu không phát hiện
3. **Upload**: Nhấn nút BOOT trên ESP32 khi upload nếu gặp lỗi
4. **Serial Monitor**: Baud rate phải là **115200**

### 🐛 Troubleshooting:

```bash
# Kiểm tra I2C devices
pio run -e test_all_sensors --target upload

# Reset PlatformIO cache
pio run --target clean
```

---

## 🎓 THÔNG TIN ĐỒ ÁN

- **Tên đồ án**: Hệ Thống Giám Sát Sức Khỏe Y Tế IoT
- **Năm**: 2026
- **Vi điều khiển**: ESP32 DevBoard
- **Framework**: Arduino + PlatformIO

---

**✨ Chúc bạn thành công với đồ án! ✨**

- **TFT ILI9341** (320x240, SPI)
- **5 nút bấm**: Up, Down, Left, Right, Select

### Sơ đồ chân kết nối

#### TFT Display (SPI)

```
TFT_CLK    -> GPIO 18
TFT_MISO   -> GPIO 19
TFT_MOSI   -> GPIO 23
TFT_CS     -> GPIO 5
TFT_DC     -> GPIO 16
TFT_RST    -> GPIO 17
```

#### Buttons

```
BTN_UP     -> GPIO 32
BTN_DOWN   -> GPIO 33
BTN_LEFT   -> GPIO 25
BTN_RIGHT  -> GPIO 26
BTN_SELECT -> GPIO 27
```

#### I2C Sensors

```
SDA -> GPIO 21
SCL -> GPIO 22
```

#### ECG Lead-Off Detection

```
LOD_PLUS  -> GPIO 13
LOD_MINUS -> GPIO 14
```

---

## 📦 Thư viện sử dụng (Dependencies)

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    SPI
    Wire
    SparkFun MAX3010x Sensor Library       # PPG/SpO2
    Adafruit MLX90614 Library @ ^2.1.5     # Temperature
    Adafruit ADS1X15 @ ^2.6.2              # ECG ADC
    ArduinoJson @ ^6.21.5                  # JSON parsing
    PubSubClient @ ^2.8                    # MQTT client
    Adafruit GFX Library @ ^1.11.5         # Graphics
    Adafruit ILI9341 @ ^1.5.12             # TFT driver
```

---

## 🔧 Cài đặt và biên dịch

### 1. Yêu cầu

- **PlatformIO IDE** (hoặc extension cho VS Code)
- **Python 3.x** (cho script ecg_monitor.py)
- Driver CH340/CP210x (tùy board ESP32)

### 2. Clone project

```bash
git clone <repository-url>
cd ESP32
```

### 3. Build & Upload

```bash
# Build firmware
pio run

# Upload lên ESP32
pio run --target upload --upload-port COM4

# Monitor Serial
pio device monitor
```

### 4. Cấu hình ban đầu

1. Sau khi upload, màn hình hiển thị menu
2. Chọn `WEB CONFIG` → ESP32 tạo Access Point
3. Kết nối WiFi: `ESP32_Config` (password: `12345678`)
4. Truy cập: `http://192.168.4.1`
5. Nhập thông tin WiFi, Email, MQTT credentials
6. Lưu → ESP32 tự động kết nối WiFi & MQTT

---

## 📊 Giám sát dữ liệu trên PC

### Sử dụng Python Script

File `ecg_monitor.py` cung cấp giao diện real-time để hiển thị:

- Sóng ECG
- Chỉ số HR, SpO2, Temperature

#### Cài đặt dependencies

```bash
pip install pyserial numpy matplotlib scipy
```

#### Chạy script

```bash
python ecg_monitor.py
```

**Lưu ý**: Sửa `COM_PORT` trong file phù hợp với cổng COM của bạn:

```python
COM_PORT = 'COM4'  # Windows
# COM_PORT = '/dev/ttyUSB0'  # Linux
```

---

## 📡 Giao thức MQTT

### Broker

- **Host**: `7280c6017830400a911fede0b97e1fed.s1.eu.hivemq.cloud`
- **Port**: 8883 (TLS)
- **Username/Password**: Cấu hình qua Web interface

### Topics

#### 1. Vital Signs

**Topic**: `health/vitals/{email}`

```json
{
  "temp": 36.5,
  "spo2": 98,
  "hr": 72,
  "timestamp": 1737964800
}
```

#### 2. ECG Data

**Topic**: `health/ecg/{email}`

```json
{
  "id": 12345,
  "data": [2048, 2050, 2049, ..., 2047]  // 100 samples
}
```

---

## 🎨 Cấu trúc code

### Thư mục

```
ESP32/
├── platformio.ini          # Cấu hình PlatformIO
├── README.md              # File này
├── include/               # Header files
│   ├── heartRate.h        # Thuật toán phát hiện nhịp tim
│   ├── signal_utils.h     # Bộ lọc tín hiệu (LPF, AGC, MA)
│   └── web_interface.h    # Giao diện Web HTML/CSS/JS
├── src/
│   ├── main.cpp           # Code chính (1111 dòng)
│   └── ecg_monitor.py     # Script Python giám sát
└── lib/                   # Thư viện tùy chỉnh (nếu có)
```

### Luồng xử lý chính (main.cpp)

1. **Setup()**:
   - Khởi tạo Serial, EEPROM, TFT
   - Đọc cấu hình WiFi/MQTT từ EEPROM
   - Khởi tạo sensors (ADS1115, MLX90614, MAX30102)
   - Kết nối WiFi & MQTT

2. **Loop()**:
   - Xử lý input từ buttons
   - Kiểm tra kết nối MQTT (auto-reconnect)
   - Gọi logic tương ứng với màn hình hiện tại:
     - `logicVital()`: Đo sinh hiệu
     - `logicECG()`: Đo ECG
     - `server.handleClient()`: Xử lý Web config

3. **logicVital()**:
   - Thu thập 100 mẫu IR/Red từ MAX30102
   - Tính SpO2 bằng thuật toán AC/DC ratio
   - Phát hiện nhịp tim từ sóng IR
   - Đọc nhiệt độ từ MLX90614
   - Hiển thị kết quả lên TFT
   - Gửi JSON qua MQTT

4. **logicECG()**:
   - Đọc ADC từ ADS1115 mỗi 8ms
   - Áp dụng LPF và AGC
   - Vẽ sóng ECG lên TFT
   - Gửi batch 100 mẫu qua MQTT

---

## 🧮 Thuật toán quan trọng

### 1. SpO2 Calculation

```cpp
// AC: Biên độ dao động
float AC_IR = IR_Max - IR_Min;
float AC_Red = Red_Max - Red_Min;

// DC: Giá trị trung bình
float DC_IR = IR_Sum / count;
float DC_Red = Red_Sum / count;

// Tỷ lệ R
float R = (AC_Red / DC_Red) / (AC_IR / DC_IR);

// SpO2 từ công thức empirical
SpO2 = 110.0 - 25.0 * R;
```

### 2. Beat Detection

```cpp
bool checkForBeat(int32_t sample) {
    // Loại bỏ DC component
    IR_AC_Signal = sample - IR_Average;

    // Phát hiện cạnh lên (positive edge)
    if (previous < 0 && current >= 0) {
        positiveEdge = true;
    }

    // Tìm đỉnh
    if (positiveEdge && current > previous) {
        IR_AC_Max = current;
    }

    // Xác nhận beat (threshold: 20-1000)
    if (IR_AC_Max > 20 && IR_AC_Max < 1000) {
        return true;  // Beat detected!
    }
}
```

### 3. Auto Gain Control

```cpp
void update(float val) {
    if (val > curMax) curMax = val;
    if (val < curMin) curMin = val;

    float range = curMax - curMin;
    if (range > minAmplitude) {
        // Decay min/max về phía giá trị trung tâm
        curMax -= range * (1.0 - decay);
        curMin += range * (1.0 - decay);
    }
}
```

---

## ⚙️ Cấu hình quan trọng

### Flash Mode

```ini
board_build.flash_mode = dout  # QUAN TRỌNG với mạch tự hàn
board_build.f_flash = 40000000L # 40MHz cho ổn định
```

### MAX30102 Settings (SpO2 Accuracy)

```cpp
byte ledBrightness = 60;   // 0x3C: Đủ sáng, không bão hòa
byte sampleAverage = 8;    // Trung bình 8 mẫu → giảm nhiễu
byte ledMode = 2;          // Red + IR
int sampleRate = 100;      // 100Hz
int pulseWidth = 411;      // 18-bit (độ phân giải cao nhất)
```

### ECG Sampling

```cpp
#define ECG_INTERVAL_US 8000  // 8ms = 125Hz
#define ECG_BATCH_SIZE 100    // Gửi 100 mẫu/lần
```

---

## 🐛 Xử lý lỗi thường gặp

### 1. **Không upload được code**

- Nhấn giữ nút BOOT trên ESP32 khi upload
- Kiểm tra driver CH340/CP210x
- Giảm `upload_speed` trong platformio.ini

### 2. **Màn hình TFT không hiển thị**

- Kiểm tra kết nối SPI (GPIO 18, 19, 23, 5, 16, 17)
- Thử đổi `tft.setRotation(1)` sang giá trị khác (0-3)

### 3. **Sensor không hoạt động**

- Kiểm tra I2C (SDA=21, SCL=22)
- Scan I2C address:
  ```cpp
  Wire.begin(21, 22);
  Wire.beginTransmission(0x57); // MAX30102
  ```

### 4. **SpO2 luôn 0 hoặc 100**

- Đảm bảo ngón tay đặt đúng vị trí, không di chuyển
- Giảm `ledBrightness` nếu bão hòa (>200000)
- Tăng `minAmplitude` trong AGC

### 5. **WiFi không kết nối**

- Kiểm tra SSID/Password trong EEPROM
- Reset cấu hình: Xóa EEPROM bằng code:
  ```cpp
  for(int i=0; i<512; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  ```

### 6. **MQTT mất kết nối**

- Kiểm tra username/password
- Tăng `keepAlive` trong client.setKeepAlive()
- Xem log Serial để debug

---

## 📈 Hiệu năng

| Thông số              | Giá trị                       |
| --------------------- | ----------------------------- |
| CPU Usage             | ~60% (dual-core)              |
| RAM Free              | ~180KB                        |
| SpO2 Update Rate      | 1 Hz                          |
| ECG Sample Rate       | 125 Hz                        |
| MQTT Publish Interval | 2s (vitals), continuous (ECG) |
| Display FPS           | ~20 FPS                       |

---

## 🔒 Bảo mật

- **TLS/SSL**: Kết nối MQTT được mã hóa
- **EEPROM**: Mật khẩu lưu dạng plaintext (nên mã hóa trong phiên bản production)
- **Web Config**: Chạy trên WiFi cục bộ, không public Internet

**Khuyến nghị**:

- Thay đổi password mặc định của MQTT
- Sử dụng VPN khi kết nối public WiFi
- Mã hóa EEPROM bằng AES trong production

---

## 🚀 Phát triển tương lai

- [ ] Thêm cảnh báo ngưỡng (threshold alerts)
- [ ] Lưu trữ dữ liệu local (SD card)
- [ ] Machine Learning để phát hiện bất thường
- [ ] Ứng dụng mobile (Flutter/React Native)
- [ ] Hỗ trợ nhiều người dùng
- [ ] Pin sạc và chế độ tiết kiệm năng lượng
- [ ] Bluetooth Low Energy (BLE) để kết nối smartphone

---

## 👨‍💻 Tác giả & Đóng góp

**Developer**: [Tên của bạn]  
**Email**: bacsi@gmail.com  
**Version**: 1.0.0  
**License**: MIT

### Đóng góp

Mọi đóng góp đều được hoan nghênh! Vui lòng:

1. Fork repository
2. Tạo branch mới (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Tạo Pull Request

---

## 📚 Tài liệu tham khảo

### Datasheets

- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [MAX30102 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX30102.pdf)
- [ADS1115 Datasheet](https://www.ti.com/lit/ds/symlink/ads1115.pdf)
- [MLX90614 Datasheet](https://www.melexis.com/-/media/files/documents/datasheets/mlx90614-datasheet-melexis.pdf)

### Papers

- [SpO2 Algorithm Theory](https://www.maximintegrated.com/en/design/technical-documents/app-notes/6/6845.html)
- [ECG Signal Processing](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6315398/)

### Tutorials

- [PlatformIO Documentation](https://docs.platformio.org/)
- [MQTT Protocol](https://mqtt.org/mqtt-specification/)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

---

## ⚠️ Lưu ý quan trọng

> **DISCLAIMER**: Thiết bị này chỉ dùng cho mục đích học tập và nghiên cứu. **KHÔNG SỬ DỤNG** cho chẩn đoán y khoa hoặc thay thế thiết bị y tế chuyên nghiệp. Luôn tham khảo ý kiến bác sĩ cho các vấn đề sức khỏe.

### An toàn điện

- Không kết nối trực tiếp với nguồn điện lưới (220V)
- Sử dụng nguồn cách ly khi đo ECG
- Tuân thủ tiêu chuẩn IEC 60601 cho thiết bị y tế nếu triển khai thực tế

---

## 📞 Hỗ trợ

Nếu gặp vấn đề, vui lòng:

1. Kiểm tra phần **Xử lý lỗi thường gặp**
2. Xem log Serial (`pio device monitor`)
3. Tạo Issue trên GitHub với thông tin:
   - Log Serial đầy đủ
   - Phiên bản PlatformIO
   - Mô tả chi tiết lỗi

---

**Happy Coding! 💙**

_Last updated: January 2026_

---

## THU VI?N

```ini
sparkfun/SparkFun MAX3010x @ ^1.1.2
adafruit/Adafruit MLX90614 @ ^2.1.5
adafruit/Adafruit ADS1X15 @ ^2.6.2
adafruit/Adafruit GFX @ ^1.11.5
adafruit/Adafruit ILI9341 @ ^1.5.12
```

---

## TH�NG TIN

- **�? �n**: H? Th?ng Gi�m S�t S?c Kh?e Y T? IoT
- **Nam**: 2026
- **Platform**: ESP32 + Arduino + PlatformIO

---

** Ch�c th�nh c�ng! **
