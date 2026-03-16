# 📊 HƯỚNG DẪN SỬ DỤNG ECG MONITOR - AD8232

> **Cập nhật:** 2/3/2026  
> **Tác giả:** ESP32 ECG Monitor Project  
> **Phiên bản:** 2.0 - Dual Mode (Serial Plotter + LCD Display)

---

## 🎯 TỔNG QUAN

Hệ thống ECG monitor có **2 chế độ hoạt động** độc lập:

| Mode               | Output          | Use Case                             | Ưu điểm                               | Nhược điểm            |
| ------------------ | --------------- | ------------------------------------ | ------------------------------------- | --------------------- |
| **Serial Plotter** | USB → PC        | Test, debug, phát triển              | Màn hình lớn, dễ debug, không cần LCD | Phải kết nối PC       |
| **LCD Display**    | TFT LCD 320x240 | Demo, presentation, thiết bị độc lập | Portable, professional UI             | Cần LCD, màn hình nhỏ |

---

## ⚙️ CÁCH CHUYỂN MODE

### 📝 Bước 1: Mở [src/main.cpp](e:\PlatformIO\ESP32\src\main.cpp)

### 📝 Bước 2: Comment/Uncomment chế độ mong muốn

```cpp
// ==========================================
// CHỌN MODE (CHỈ BẬT 1 DÒNG)
// ==========================================
#define ECG_MODE_SERIAL_PLOTTER  // Mode 1: Serial Plotter
// #define ECG_MODE_LCD_DISPLAY  // Mode 2: LCD Display
```

**Lưu ý:** CHỈ bật 1 mode tại 1 thời điểm!

### 📝 Bước 3: Upload code lên ESP32

```bash
# PlatformIO
pio run -e Main -t upload

# Hoặc: Click Upload button trong PlatformIO
```

---

## 📊 MODE 1: SERIAL PLOTTER

### ✅ Khi nào dùng?

- ✅ Đang phát triển/test code
- ✅ Muốn xem chi tiết tín hiệu ECG trên màn hình lớn
- ✅ Cần debug filter/processing algorithm
- ✅ Muốn record/export data
- ✅ Chưa có LCD hoặc đang test breadboard

### 🔧 Cấu hình

**File:** [src/main.cpp](e:\PlatformIO\ESP32\src\main.cpp)

```cpp
#define ECG_MODE_SERIAL_PLOTTER  // ✅ Bật dòng này
// #define ECG_MODE_LCD_DISPLAY  // ❌ Comment dòng này
```

### 📺 Cách xem đồ thị

#### **Option 1: PlatformIO Serial Monitor** (Khuyến nghị)

1. Click **Serial Monitor** icon trong PlatformIO
2. Baudrate: 115200 (tự động set)
3. Bạn sẽ thấy output:
   ```
   Raw:125.34 Filtered:123.12 HR:72
   Raw:126.01 Filtered:123.45 HR:72
   Raw:124.88 Filtered:122.98 HR:72
   ```

#### **Option 2: Arduino IDE Serial Plotter**

1. Mở Arduino IDE
2. Tools → Serial Plotter
3. Baudrate: 115200
4. Đồ thị sẽ tự động vẽ 3 đường:
   - **Raw** (màu đỏ): Tín hiệu thô từ AD8232
   - **Filtered** (màu xanh): Tín hiệu sau khi lọc nhiễu
   - **HR** (màu vàng): Heart Rate (BPM)

#### **Option 3: TeleplotG (Advanced)**

TeleplotG là Chrome extension chuyên vẽ đồ thị realtime từ Serial data.

**Cài đặt:**

1. Chrome Web Store → Tìm "TeleplotG"
2. Install extension
3. Mở TeleplotG → Connect to Serial Port
4. Baudrate: 115200

**Ưu điểm:**

- Đồ thị đẹp hơn Arduino Plotter
- Có thể zoom, pause, export data
- Hỗ trợ nhiều chart đồng thời

### 📊 Format dữ liệu

```
Raw:123.45 Filtered:120.30 HR:75
```

- **Raw:** Tín hiệu ECG thô (mV) - chưa lọc
- **Filtered:** Tín hiệu sau 3-layer filter (mV)
- **HR:** Heart Rate (BPM)

**Lưu ý:**

- Format `Label:Value` tự động được parse bởi Serial Plotter
- Nếu điện cực rời: `Raw:0.00 Filtered:0.00 HR:0`

### 🔍 Debug Info (mỗi 1 giây)

```
💓 Heart Rate: 72 BPM

⚠️  WARNING: Leads not connected!
   Check RED, YELLOW, GREEN electrodes
```

### ⚡ Performance

- **Tần số:** 250 Hz (4ms/sample) - chuẩn ECG
- **Baudrate:** 115200 bps
- **Latency:** < 5ms

**Nếu bị lag:**

- Tăng baudrate lên 230400 hoặc 500000 (sửa trong setup())
- Giảm tần số xuống 100Hz (sửa ECG_UPDATE_INTERVAL = 10)

---

## 🖥️ MODE 2: LCD DISPLAY

### ✅ Khi nào dùng?

- ✅ Demo cho thầy/bạn/hội đồng
- ✅ Presentation, báo cáo khóa luận
- ✅ Muốn thiết bị standalone (không cần PC)
- ✅ Giao diện professional (medical-grade UI)

### 🔧 Cấu hình

**File:** [src/main.cpp](e:\PlatformIO\ESP32\src\main.cpp)

```cpp
// #define ECG_MODE_SERIAL_PLOTTER  // ❌ Comment dòng này
#define ECG_MODE_LCD_DISPLAY        // ✅ Bật dòng này
```

### 🎨 Giao diện

```
┌────────────────────────────────────────┐
│ ECG MONITOR               [●] LIVE    │ ← Header (25px)
├────────────────────────────────────────┤
│                                        │
│  ╱╲    ╱╲    ╱╲    ╱╲    ╱╲          │
│ ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲         │ ← ECG Chart (165px)
│      ╲╱    ╲╱    ╲╱    ╲╱    ╲╱       │   Realtime waveform
│                                        │   Grid background
│                                        │
├────────────────────────────────────────┤
│ HEART RATE                             │ ← Bottom Bar (47px)
│                                        │
│    72                              BPM │   Big number display
└────────────────────────────────────────┘
```

### 🎮 Buttons (Future features - chưa implement)

| Button | Function                        | Status         |
| ------ | ------------------------------- | -------------- |
| UP     | Zoom in / Increase sensitivity  | 🔜 Coming soon |
| DOWN   | Zoom out / Decrease sensitivity | 🔜 Coming soon |
| LEFT   | Back to menu                    | 🔜 Coming soon |
| RIGHT  | Next screen                     | 🔜 Coming soon |
| ENTER  | Confirm / Menu                  | 🔜 Coming soon |

**Hiện tại:** UI chỉ hiển thị ECG waveform realtime, chưa có menu navigation.

### 📐 Specifications

**LCD:**

- Size: 320x240 pixels
- Driver: ILI9341 (TFT_eSPI)
- Orientation: Landscape (rotation 3)
- SPI Speed: 27 MHz

**ECG Chart:**

- Points: 150 buffer
- Range: 0-100 (normalized)
- Update rate: 250 Hz
- Display time: ~6 seconds @ 25Hz, 0.6s @ 250Hz

**Colors:**

- Background: Black (#000000)
- Waveform: Green (#00E676) - Medical standard
- Grid: Dark green (#1a331a) - ECG paper style
- Heart Rate: Red (#FF1744)
- Status text: Light gray (#999999)

### ⚠️ Lead-off Detection

Khi điện cực rời:

```
┌────────────────────────────────────────┐
│ ECG MONITOR               [●] LIVE    │
├────────────────────────────────────────┤
│                                        │
│                                        │
│     ⚠ LEADS NOT CONNECTED             │  ← Warning message
│                                        │
│                                        │
│                                        │
├────────────────────────────────────────┤
│ HEART RATE                             │
│                                        │
│    --                              BPM │  ← "--" khi không có tín hiệu
└────────────────────────────────────────┘
```

### ⚡ Performance

- **Waveform update:** 250 Hz (4ms)
- **UI labels update:** 10 Hz (100ms)
- **LVGL handler:** 200 Hz (5ms)
- **RAM usage:** ~40KB (LVGL buffer + chart)

---

## 🔌 HARDWARE SETUP (Cả 2 modes)

### 📋 Component List

| Component               | Quantity | Note                 |
| ----------------------- | -------- | -------------------- |
| ESP32-WROOM-32D         | 1        | Microcontroller      |
| AD8232 ECG Module       | 1        | Analog front-end     |
| ADS1115 16-bit ADC      | 1        | I2C address 0x48     |
| TFT LCD ILI9341         | 1        | Chỉ cần cho LCD mode |
| ECG Electrodes (3-lead) | 1 set    | Red, Yellow, Green   |
| Breadboard              | 1        | For prototyping      |
| Jumper wires            | ~20      |                      |

### 🔗 Wiring

**AD8232 → ESP32:**

```
AD8232 OUTPUT → ADS1115 A0
AD8232 LO+    → GPIO 13
AD8232 LO-    → GPIO 14
AD8232 3.3V   → 3.3V
AD8232 GND    → GND
```

**ADS1115 → ESP32:**

```
ADS1115 SDA   → GPIO 21
ADS1115 SCL   → GPIO 22
ADS1115 VDD   → 3.3V
ADS1115 GND   → GND
ADS1115 ADDR  → GND (address 0x48)
```

**TFT LCD → ESP32** (chỉ cho LCD mode):

```
TFT MOSI      → GPIO 23
TFT MISO      → GPIO 19
TFT SCK       → GPIO 18
TFT CS        → GPIO 15
TFT DC        → GPIO 16
TFT RST       → GPIO 17
TFT VCC       → 3.3V hoặc 5V (tùy LCD)
TFT GND       → GND
TFT LED       → 3.3V (backlight)
```

**Buttons** (optional, cho future features):

```
BTN_UP        → GPIO 32 (với pull-up)
BTN_DOWN      → GPIO 33 (với pull-up)
BTN_LEFT      → GPIO 25 (với pull-up)
BTN_RIGHT     → GPIO 26 (với pull-up)
BTN_ENTER     → GPIO 27 (với pull-up)
```

Chi tiết xem: [AD8232_WIRING.md](e:\PlatformIO\ESP32\AD8232_WIRING.md)

### 🩺 Electrode Placement

**3-Lead ECG (Lead I configuration):**

```
        RED (RA)              YELLOW (LA)
      Right Arm               Left Arm
         ●                        ●
         |                        |
    Cổ tay phải            Cổ tay trái
         |                        |
         └────────────────────────┘
                    |
                    ● GREEN (RL)
                  Right Leg
                 Mắt cá chân
               hoặc phía bụng
```

**Wire colors (International standard):**

- **RED:** Right Arm (RA) - Cổ tay phải
- **YELLOW:** Left Arm (LA) - Cổ tay trái
- **GREEN:** Right Leg (RL) - Ground/Reference

**Cách gắn:**

1. Làm sạch da bằng cồn
2. Dán electrode chặt, không bị nhăn
3. Kẹp cable vào electrode
4. Giữ yên, không cử động

Chi tiết xem: [AD8232_WIRING.md](e:\PlatformIO\ESP32\AD8232_WIRING.md)

---

## 🧪 TESTING WORKFLOW

### ✅ Checklist trước khi test

- [ ] Đã chọn mode (Serial Plotter hoặc LCD)
- [ ] Upload code thành công
- [ ] Kết nối đúng I2C (SDA=21, SCL=22)
- [ ] Kết nối đúng LO+/LO- (GPIO 13/14)
- [ ] Nếu LCD mode: Kết nối đúng SPI LCD
- [ ] 3 điện cực đã dán tốt lên da
- [ ] Serial Monitor đã mở (baudrate 115200)

### 📝 Test Step-by-step

**1. Power ON**

```
╔════════════════════════════════════════════════════╗
║     ECG MONITOR - AD8232 + ADS1115               ║
╚════════════════════════════════════════════════════╝

📊 MODE: SERIAL PLOTTER  # hoặc LCD DISPLAY
💓 Initializing AD8232...
✓ AD8232 ready
✓ System ready!
```

**2. Kiểm tra Lead-off**

Chưa gắn điện cực:

```
⚠️  WARNING: Leads not connected!
   Check RED, YELLOW, GREEN electrodes
```

**3. Gắn điện cực**

Sau khi gắn đúng:

```
💓 Heart Rate: 72 BPM
```

**4. Quan sát waveform**

- **Serial Plotter:** Thấy 3 đường ECG dao động
- **LCD:** Thấy waveform màu xanh trên màn hình

**5. Verify Heart Rate**

- Đếm tay: ~60-100 BPM (người bình thường)
- So sánh với Phone app hoặc đồng hồ
- Chờ 10-15s để thuật toán ổn định

---

## 🐛 TROUBLESHOOTING

### ❌ Problem: "No mode selected!" compile error

**Nguyên nhân:** Chưa bật mode nào trong main.cpp

**Giải pháp:**

```cpp
// Bật 1 trong 2 dòng này
#define ECG_MODE_SERIAL_PLOTTER  // ✅
// #define ECG_MODE_LCD_DISPLAY
```

---

### ❌ Problem: Serial Plotter không hiển thị đồ thị

**Nguyên nhân:**

- Baudrate sai
- Format output không đúng
- Port chưa connect

**Giải pháp:**

1. Kiểm tra baudrate: 115200
2. Kiểm tra format: `Raw:xxx Filtered:xxx HR:xxx`
3. Reset ESP32 và reconnect Serial Monitor
4. Thử Arduino IDE Serial Plotter thay vì PlatformIO

---

### ❌ Problem: LCD blank screen

**Nguyên nhân:**

- TFT_eSPI config sai
- Kết nối SPI sai
- LCD chưa có nguồn

**Giải pháp:**

1. Kiểm tra [platformio.ini](e:\PlatformIO\ESP32\platformio.ini) - ILI9341 driver
2. Kiểm tra wiring: MOSI=23, SCK=18, CS=15, DC=16, RST=17
3. Kiểm tra backlight LED có sáng không
4. Test LCD bằng TFT_eSPI example (`TFT_Rainbow`)

---

### ❌ Problem: ECG waveform không đúng/nhiều nhiễu

**Nguyên nhân:**

- Electrode dán lỏng
- Ground kém
- Nhiễu 50Hz từ điện lưới
- Cable quá dài
- ADC gain sai

**Giải pháp:**

1. **Electrode:** Dán chặt, làm sạch da bằng cồn
2. **Ground:** Đảm bảo GND chung giữa ESP32, AD8232, ADS1115
3. **Shielding:** Dùng cable ngắn, tránh xa nguồn switching
4. **Filter:** Code đã có 3-layer filter, chờ 5-10s ổn định
5. **Check ADC settings:**
   ```cpp
   // Trong ad8232.cpp
   ads.setGain(GAIN_TWOTHIRDS);  // ±6.144V
   ads.setDataRate(RATE_ADS1115_250SPS);  // 250Hz
   ```

---

### ❌ Problem: Heart Rate không chính xác

**Nguyên nhân:**

- Thuật toán chưa ổn định
- Tín hiệu ECG yếu
- Nhiễu quá lớn
- Electrode placement sai

**Giải pháp:**

1. **Đợi 10-15 giây** sau khi gắn electrode
2. **Giữ yên**, không cử động
3. **Kiểm tra waveform** trên Serial Plotter:
   - Phải thấy rõ QRS complex (peak cao đột ngột)
   - Peak phải cách đều nhau (~1s với HR 60 BPM)
4. **Re-position electrodes** theo đúng hướng dẫn
5. **Verify bằng tay:** Đếm mạch 15s x 4 = BPM

---

### ❌ Problem: "Leads not connected" cảnh báo liên tục

**Nguyên nhân:**

- Electrode thật sự chưa gắn
- AD8232 LO+/LO- không kết nối đúng
- Electrode hết gel
- Da quá khô

**Giải pháp:**

1. Kiểm tra wiring: LO+ → GPIO 13, LO- → GPIO 14
2. Kiểm tra electrode: Phải có gel dính
3. Thử electrode mới
4. Làm ẩm da bằng nước/cồn

---

### ❌ Problem: LCD mode - UI bị lag/stutter

**Nguyên nhân:**

- LVGL buffer quá nhỏ
- Update frequency quá cao
- RAM không đủ

**Giải pháp:**

1. **Giảm update rate:**

   ```cpp
   #define ECG_UPDATE_INTERVAL 10  // 100Hz thay vì 250Hz
   ```

2. **Tăng LVGL buffer** (trong [ui_ecg.cpp](e:\PlatformIO\ESP32\src\ui_ecg.cpp)):

   ```cpp
   static lv_color_t buf[SCREEN_WIDTH * 30]; // 30 lines thay vì 20
   ```

3. **Giảm chart points:**
   ```cpp
   lv_chart_set_point_count(chart_ecg, 100); // 100 thay vì 150
   ```

---

## 📚 FILES REFERENCE

| File                                                                                   | Mô tả                          |
| -------------------------------------------------------------------------------------- | ------------------------------ |
| [src/main.cpp](e:\PlatformIO\ESP32\src\main.cpp)                                       | Main program - Chọn mode ở đây |
| [src/ad8232.cpp](e:\PlatformIO\ESP32\src\ad8232.cpp)                                   | AD8232 driver + filtering      |
| [include/ad8232.h](e:\PlatformIO\ESP32\include\ad8232.h)                               | AD8232 API                     |
| [src/ui_ecg.cpp](e:\PlatformIO\ESP32\src\ui_ecg.cpp)                                   | ECG UI riêng (LCD mode)        |
| [include/ui_ecg.h](e:\PlatformIO\ESP32\include\ui_ecg.h)                               | ECG UI header                  |
| [platformio.ini](e:\PlatformIO\ESP32\platformio.ini)                                   | Build config                   |
| [AD8232_README.md](e:\PlatformIO\ESP32\AD8232_README.md)                               | AD8232 technical docs          |
| [AD8232_WIRING.md](e:\PlatformIO\ESP32\AD8232_WIRING.md)                               | Wiring diagrams                |
| [GPIO_PIN_MAPPING.md](e:\PlatformIO\ESP32\GPIO_PIN_MAPPING.md)                         | All GPIO pins                  |
| [BREADBOARD_TESTING_CHECKLIST.md](e:\PlatformIO\ESP32\BREADBOARD_TESTING_CHECKLIST.md) | Testing guide                  |

---

## 🎓 TECHNICAL DETAILS

### ECG Signal Processing Pipeline

```
AD8232 Analog ECG
      ↓
ADS1115 16-bit ADC (250 Hz)
      ↓
Moving Average Filter (4 samples)  ← Remove high-freq noise
      ↓
Median Filter (5 samples)           ← Remove spike artifacts
      ↓
High-Pass Filter (0.5 Hz cutoff)    ← Remove baseline drift
      ↓
Filtered ECG Signal
      ↓
Peak Detection Algorithm
      ↓
Heart Rate (BPM)
```

### Filters Configuration

**1. Moving Average Filter**

- Window: 4 samples
- Effect: Smoothing, reduce high-frequency noise
- Cutoff: ~62 Hz @ 250 Hz sampling

**2. Median Filter**

- Window: 5 samples
- Effect: Remove impulse noise, spikes
- Non-linear filter

**3. High-Pass Filter (IIR)**

- Type: First-order IIR
- Cutoff: 0.5 Hz
- Alpha: 0.99
- Effect: Remove DC offset, baseline drift

**Formula:**

```
y[n] = α * (y[n-1] + x[n] - x[n-1])
```

### Heart Rate Detection

**Algorithm:**

1. Adaptive threshold
2. Peak detection (QRS complex)
3. R-R interval measurement
4. Moving average of last 5 intervals
5. BPM = 60,000 / average_interval_ms

**Valid range:** 40-200 BPM

### Performance Metrics

| Metric             | Value                     |
| ------------------ | ------------------------- |
| Sampling rate      | 250 Hz                    |
| ADC resolution     | 16-bit (0.1875 mV/bit)    |
| ECG range          | ±3 mV (typical)           |
| HR accuracy        | ±5%                       |
| Lead-off detection | < 100ms                   |
| Filter delay       | ~20ms (5 samples @ 250Hz) |

---

## 📞 SUPPORT & CONTACT

**Nếu gặp vấn đề:**

1. Đọc kỹ phần Troubleshooting
2. Check [BREADBOARD_TESTING_CHECKLIST.md](e:\PlatformIO\ESP32\BREADBOARD_TESTING_CHECKLIST.md)
3. Review wiring trong [AD8232_WIRING.md](e:\PlatformIO\ESP32\AD8232_WIRING.md)
4. Kiểm tra Serial Monitor debug messages

**For development:**

- GitHub: (your repo)
- Email: (your email)
- Documentation: Các file .md trong project

---

## ✅ QUICK START CHECKLIST

**Hardware:**

- [ ] ESP32 kết nối USB
- [ ] AD8232 + ADS1115 đã đấu dây đúng
- [ ] (LCD mode) TFT LCD đã kết nối
- [ ] Electrode đã dán lên da

**Software:**

- [ ] Code đã upload thành công
- [ ] Đã chọn đúng mode trong main.cpp
- [ ] Serial Monitor đã mở (115200)

**Test:**

- [ ] Thấy "System ready!" trên Serial
- [ ] Lead-off detection hoạt động (rút dây → warning)
- [ ] ECG waveform hiển thị (Serial Plotter hoặc LCD)
- [ ] Heart rate detection chính xác (±5 BPM)

---

**Chúc bạn test thành công! 🚀**

Nếu cần hỗ trợ thêm, tham khảo các file documentation khác trong project.
