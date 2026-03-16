# 📌 GPIO PIN MAPPING - ESP32 ECG Monitor

> **QUAN TRỌNG:** File này ghi lại TẤT CẢ GPIO đã sử dụng  
> **MỤC ĐÍCH:** Tránh conflict pins khi thiết kế PCB  
> **CẬP NHẬT:** Mỗi khi thêm/bớt sensor hoặc thay đổi kết nối

---

## 🔌 CURRENT PIN USAGE (Hiện tại đang dùng)

### **I2C (Wire)**

| Pin     | Function | Connected To                | Note          |
| ------- | -------- | --------------------------- | ------------- |
| GPIO 21 | SDA      | ADS1115, MAX30102, MLX90614 | Pull-up 4.7kΩ |
| GPIO 22 | SCL      | ADS1115, MAX30102, MLX90614 | Pull-up 4.7kΩ |

### **SPI (TFT LCD)**

| Pin     | Function | Connected To | Note         |
| ------- | -------- | ------------ | ------------ |
| GPIO 23 | MOSI     | TFT LCD      |              |
| GPIO 19 | MISO     | TFT LCD      |              |
| GPIO 18 | SCK      | TFT LCD      |              |
| GPIO 15 | CS       | TFT LCD      | Chip Select  |
| GPIO 16 | DC       | TFT LCD      | Data/Command |
| GPIO 17 | RST      | TFT LCD      | Reset        |

### **AD8232 ECG**

| Pin     | Function | Connected To      | Note                    |
| ------- | -------- | ----------------- | ----------------------- |
| GPIO 13 | LO+      | AD8232 Lead-Off + | Input, internal pull-up |
| GPIO 14 | LO-      | AD8232 Lead-Off - | Input, internal pull-up |

### **Buttons (Tùy chọn - nếu có)**

| Pin     | Function  | Connected To | Note                |
| ------- | --------- | ------------ | ------------------- |
| GPIO 32 | BTN_UP    | Button       | Pull-up, active LOW |
| GPIO 33 | BTN_DOWN  | Button       | Pull-up, active LOW |
| GPIO 25 | BTN_LEFT  | Button       | Pull-up, active LOW |
| GPIO 26 | BTN_RIGHT | Button       | Pull-up, active LOW |
| GPIO 27 | BTN_ENTER | Button       | Pull-up, active LOW |

### **LED Indicators (Khuyến nghị thêm)**

| Pin     | Function    | Connected To | Note               |
| ------- | ----------- | ------------ | ------------------ |
| GPIO 2  | LED_BUILTIN | Onboard LED  | Active HIGH (test) |
| GPIO 4  | LED_POWER   | Green LED    | Power ON indicator |
| GPIO 5  | LED_WIFI    | Blue LED     | WiFi connected     |
| GPIO 12 | LED_ECG     | Red LED      | ECG signal OK      |

### **Power & Ground**

| Pin | Function     | Note                        |
| --- | ------------ | --------------------------- |
| 3V3 | Power Out    | Max 600mA total             |
| 5V  | USB Power In | From USB or external        |
| GND | Ground       | Multiple GND pins available |
| EN  | Enable       | Pull-up 10kΩ for auto-boot  |

---

## ⚠️ RESERVED PINS (Không được dùng)

### **Strapping Pins (Boot mode)**

| Pin     | Purpose   | Constraint                                  |
| ------- | --------- | ------------------------------------------- |
| GPIO 0  | Boot Mode | Must be HIGH during boot (internal pull-up) |
| GPIO 2  | Boot Mode | Must be floating or LOW during boot         |
| GPIO 5  | Boot Mode | Must be HIGH during boot                    |
| GPIO 12 | Boot Mode | Must be LOW during boot (affects voltage)   |
| GPIO 15 | Boot Mode | Must be HIGH during boot                    |

**⚠️ Lưu ý:** Các pin này CÓ THỂ dùng NHƯNG phải cẩn thận:

- Không kéo xuống GND khi boot
- Không có pull-down nặng
- Tốt nhất: Tránh dùng cho critical functions

### **Input Only Pins**

| Pin          | Note                             |
| ------------ | -------------------------------- |
| GPIO 34      | Input only, no pull-up/pull-down |
| GPIO 35      | Input only, no pull-up/pull-down |
| GPIO 36 (VP) | Input only, ADC1_CH0             |
| GPIO 39 (VN) | Input only, ADC1_CH3             |

**⚠️ Lưu ý:** Chỉ dùng làm input, không thể output

### **Flash & PSRAM Pins (KHÔNG BAO GIỜ DÙNG)**

| Pin       | Purpose                         |
| --------- | ------------------------------- |
| GPIO 6-11 | Kết nối Flash SPI (DO NOT USE!) |

---

## 📊 PIN AVAILABILITY SUMMARY

**Total GPIO pins:** 34  
**Reserved (Flash):** 6 pins (GPIO 6-11)  
**Strapping pins:** 6 pins (GPIO 0, 2, 5, 12, 15, GPIO_BOOT)  
**Input-only:** 4 pins (GPIO 34, 35, 36, 39)  
**Currently used:** 13 pins  
**Available for future:** ~5-10 pins

---

## 🎯 AVAILABLE PINS FOR EXPANSION

**Safe to use (Good for general purpose):**

- GPIO 4 ✓
- GPIO 5 (nếu không dùng boot mode)
- GPIO 12 (nếu không dùng boot mode)
- GPIO 14 ✓ (đang dùng LO-)
- GPIO 25 ✓
- GPIO 26 ✓
- GPIO 27 ✓
- GPIO 32 ✓
- GPIO 33 ✓

**ADC-capable pins (nếu cần đọc analog):**

- GPIO 32 (ADC1_CH4)
- GPIO 33 (ADC1_CH5)
- GPIO 34 (ADC1_CH6, input-only)
- GPIO 35 (ADC1_CH7, input-only)
- GPIO 36 (ADC1_CH0, input-only)
- GPIO 39 (ADC1_CH3, input-only)

**Touch-capable pins (nếu cần touch sensor):**

- GPIO 4 (TOUCH0)
- GPIO 12 (TOUCH5)
- GPIO 13 (TOUCH4) - đang dùng LO+
- GPIO 14 (TOUCH6) - đang dùng LO-
- GPIO 27 (TOUCH7)
- GPIO 32 (TOUCH9)
- GPIO 33 (TOUCH8)

---

## 🔍 I2C DEVICE ADDRESSES

| Device   | I2C Address | Alternative Address     | Note           |
| -------- | ----------- | ----------------------- | -------------- |
| ADS1115  | 0x48        | 0x49 (ADDR→VDD)         | 16-bit ADC     |
| MAX30102 | 0x57        | Fixed                   | SpO2 sensor    |
| MLX90614 | 0x5A        | Configurable via EEPROM | IR thermometer |

**⚠️ Lưu ý:**

- Đảm bảo KHÔNG có conflict address
- Nếu cần 2 ADS1115 → dùng 0x48 và 0x49

---

## 📐 SCHEMATIC NOTES (Cho PCB sau này)

### **Pull-up/Pull-down Requirements:**

```
ESP32 EN:       10kΩ pull-up to 3.3V
I2C SDA/SCL:    4.7kΩ pull-up to 3.3V
GPIO 0:         10kΩ pull-up (boot mode)
Buttons:        Internal pull-up (enabled in code)
```

### **Decoupling Capacitors:**

```
ESP32 3.3V:     100nF + 10µF (sát chân)
ADS1115 VDD:    100nF + 10µF
MAX30102 VDD:   100nF + 1µF
MLX90614 VDD:   100nF
TFT LCD VCC:    100nF + 10µF
Power input:    100µF + 10µF
```

### **ESD Protection (Khuyến nghị cho ECG):**

```
ECG inputs:     TVS diode + 1MΩ resistor
I2C lines:      Optional: 330Ω series resistor
```

---

## 🛠️ POWER BUDGET CALCULATION

| Component         | Typical Current | Max Current | Note                   |
| ----------------- | --------------- | ----------- | ---------------------- |
| ESP32 WiFi ON     | 160-260mA       | 350mA       | Peak when transmitting |
| ESP32 WiFi OFF    | 80mA            | 100mA       |                        |
| TFT LCD Backlight | 20-100mA        | 150mA       | Adjustable             |
| ADS1115           | 150µA           | 200µA       | Very low               |
| MAX30102          | 600µA-1.2mA     | 50mA        | LED current adjustable |
| MLX90614          | 1.5mA           | 2mA         |                        |
| AD8232            | 170µA           | 200µA       | Very low               |
| **TOTAL**         | **~300mA**      | **~500mA**  |                        |

**Nguồn cần:**

- USB 5V → LDO 3.3V (AMS1117 - max 800mA) ✓ OK
- Hoặc: ESP32 onboard regulator (max 600mA) → hơi chật

**Khuyến nghị:** Dùng LDO riêng cho ổn định hơn

---

## 📝 MODIFICATION LOG

### Version 1.0 (Hiện tại - Breadboard)

**Date:** **_/_**/2026

- [x] I2C: GPIO 21 (SDA), GPIO 22 (SCL)
- [x] SPI LCD: GPIO 23, 19, 18, 15, 16, 17
- [x] AD8232: GPIO 13 (LO+), GPIO 14 (LO-)
- [ ] Buttons: GPIO 32, 33, 25, 26, 27 (chưa implement)
- [ ] LEDs: GPIO 4, 5, 12 (chưa thêm)

### Version 1.1 (Dự kiến - Perfboard)

**Date:** **_/_**/2026

- [ ] Thêm LED indicators
- [ ] Thêm buttons (nếu cần)
- [ ] Optimize pin usage

### Version 2.0 (PCB Final)

**Date:** **_/_**/2026

- [ ] Final pin assignment
- [ ] ESD protection
- [ ] Proper routing

---

## ⚙️ PLATFORMIO.INI BUILD FLAGS

**Để reference trong code:**

```ini
build_flags =
    ; I2C Pins
    -D I2C_SDA=21
    -D I2C_SCL=22

    ; SPI LCD Pins
    -D TFT_MISO=19
    -D TFT_MOSI=23
    -D TFT_SCLK=18
    -D TFT_CS=15
    -D TFT_DC=16
    -D TFT_RST=17

    ; AD8232 Pins
    -D LO_PLUS_PIN=13
    -D LO_MINUS_PIN=14

    ; Button Pins (optional)
    -D BTN_UP=32
    -D BTN_DOWN=33
    -D BTN_LEFT=25
    -D BTN_RIGHT=26
    -D BTN_ENTER=27
```

---

## 🎨 PIN DIAGRAM (ASCII Art)

```
                    ESP32-WROOM-32

         3V3  [ ] [ ] GND
     EN      [ ] [ ] GPIO 23 ──── MOSI (LCD)
  VP/ADC0    [ ] [ ] GPIO 22 ──── SCL (I2C)
  VN/ADC3    [ ] [ ] TX
         34  [ ] [ ] RX
         35  [ ] [ ] GPIO 21 ──── SDA (I2C)
         32  [ ] [ ] GPIO 19 ──── MISO (LCD)
         33  [ ] [ ] GPIO 18 ──── SCK (LCD)
         25  [ ] [ ] GPIO 5  ──── (LED_WIFI)
         26  [ ] [ ] GPIO 17 ──── RST (LCD)
         27  [ ] [ ] GPIO 16 ──── DC (LCD)
         14  [ ] [ ] GPIO 4  ──── (LED_POWER)
    (LO-)12  [ ] [ ] GPIO 0
    (LO+)13  [ ] [ ] GPIO 2
        GND  [ ] [ ] GPIO 15 ──── CS (LCD)
        5V   [ ] [ ] GND
```

---

## ✅ PRE-PCB DESIGN CHECKLIST

Trước khi thiết kế PCB, đảm bảo:

- [ ] Đã test tất cả pins trên breadboard
- [ ] Không có conflict pins
- [ ] Không dùng GPIO 6-11 (Flash)
- [ ] Strapping pins được xử lý đúng
- [ ] I2C có pull-up resistor
- [ ] Power budget đủ
- [ ] Đã document đầy đủ trong file này
- [ ] Schematic đã vẽ chính xác theo breadboard
- [ ] Đã review với thầy/bạn

---

## 📞 QUICK REFERENCE

**Khi cần thêm sensor/peripheral:**

1. Check bảng "AVAILABLE PINS" ở trên
2. Tránh Strapping pins nếu có thể
3. Tránh GPIO 6-11
4. Update file này
5. Test trên breadboard
6. Document changes

**Khi gặp lỗi boot:**

- Check GPIO 0, 2, 5, 12, 15
- Đảm bảo không có pull-down nặng
- Disconnect sensors và test lại

**Khi cần debug I2C:**

```cpp
Wire.begin(21, 22);
for (byte i = 0; i < 127; i++) {
  Wire.beginTransmission(i);
  if (Wire.endTransmission() == 0) {
    Serial.printf("Found: 0x%02X\n", i);
  }
}
```

---

**CẬP NHẬT FILE NÀY MỖI KHI THAY ĐỔI PHẦN CỨNG!** 📝
