# 🩺 ECG TROUBLESHOOTING GUIDE - HƯỚNG DẪN SỬA LỖI

> **Ngày:** 2/3/2026  
> **Vấn đề:** Tín hiệu ECG yếu, LCD chỉ đường thẳng, Heart Rate không detect

---

## 🔴 TRIỆU CHỨNG BẠN ĐANG GẶP:

```
Raw:-0.19,Filtered:0.00,HR:0          ← Signal quá yếu
[E][Wire.cpp:513] Error 263           ← I2C error
[DEBUG] HR:0 | ECG:0.00 mV            ← Không detect HR
LCD: Chỉ thấy đường thẳng             ← Không có waveform
```

---

## 🔍 CHẨN ĐOÁN NGUYÊN NHÂN:

### 1. **Tín hiệu quá yếu (Raw: -0.19 mV)**

**Nguyên nhân có thể:**

- ❌ AD8232 OUTPUT không kết nối vào ADS1115 A0
- ❌ Điện cực chất lượng kém / không có gel
- ❌ Dán điện cực không chặt
- ❌ Da không sạch (có mồ hôi, dầu)
- ❌ Cable electrode quá dài / breadboard jumper wire

**ECG signal chuẩn:**

- ✅ Raw: ±0.5 đến ±5 mV (trung bình ±1-2 mV)
- ✅ Có QRS complex rõ ràng
- ✅ Dao động liên tục với nhịp tim

---

### 2. **I2C Error 263 liên tục**

**Nguyên nhân:**

- ADS1115 kết nối lỏng
- I2C cable quá dài
- Thiếu pull-up resistor (module thường có sẵn)
- Voltage không đủ

**Đã fix:**

- ✅ Giảm I2C clock: 400kHz → 100kHz
- ✅ Tăng timeout: 2 giây
- ⏳ Cần rebuild: `pio run -e Main -t clean`

---

### 3. **LCD chỉ hiển thị đường thẳng**

**Nguyên nhân:**

- Normalization range quá lớn (-3mV to +3mV)
- Signal -0.19mV khi map vào range 0-100 → gần 50 (baseline)
- Không thấy dao động

**Đã fix:**

- ✅ Giảm range: ±3mV → ±1mV
- Chart sẽ nhạy hơn 3x

---

## 🔧 HƯỚNG DẪN SỬA TỪNG BƯỚC:

### BƯỚC 1: REBUILD CODE (BẮT BUỘC)

```powershell
# Vào thư mục project
cd e:\PlatformIO\ESP32

# Clean build (xóa cache cũ)
pio run -e Main -t clean

# Build lại từ đầu
pio run -e Main -t upload

# Monitor
pio device monitor
```

**Kiểm tra:**

- ✅ KHÔNG còn `[E][Wire.cpp:513]` errors
- ✅ KHÔNG còn `[DEBUG]` messages
- ✅ Chỉ thấy: `Raw:xxx,Filtered:xxx,HR:xxx`

**Nếu vẫn có errors:**
→ Rebuild chưa apply CORE_DEBUG_LEVEL=0  
→ Làm lại: `pio run -e Main -t clean` rồi upload

---

### BƯỚC 2: KIỂM TRA RAW ADC VALUE

Sau khi upload xong, Serial Monitor sẽ hiện:

```
[ADC_RAW]1234
```

**Phân tích ADC value:**

| ADC Value         | Ý nghĩa                             | Hành động                  |
| ----------------- | ----------------------------------- | -------------------------- |
| **0 hoặc gần 0**  | AD8232 CHƯA kết nối vào ADS1115     | Kiểm tra wiring            |
| **-2 đến +2**     | Có kết nối nhưng không có electrode | Gắn electrode              |
| **-100 đến +100** | Có signal yếu                       | Kiểm tra electrode quality |
| **-500 đến +500** | ✅ Signal tốt, ECG đúng             | OK!                        |
| **> ±2000**       | Quá tải, có thể sai wiring          | Kiểm tra VCC               |

---

### BƯỚC 3: KIỂM TRA HARDWARE

#### **A. ADS1115 I2C Connection:**

```
ADS1115     → ESP32
───────────────────────
VDD (VCC)   → 3.3V ✅ (KHÔNG dùng 5V!)
GND         → GND
SCL         → GPIO 22
SDA         → GPIO 21
ADDR        → GND (address 0x48)
```

**Test I2C:**

```powershell
# Install i2c-tools (optional)
pio device monitor

# Trong code, sẽ tự detect I2C address
```

**Kiểm tra pull-up resistors:**

- Module ADS1115 có 2 con trở nhỏ (4.7kΩ) gần chân SDA/SCL
- Nếu không có → cần hàn thêm pull-up 4.7kΩ

---

#### **B. AD8232 → ADS1115 Connection:**

**QUAN TRỌNG:**

```
AD8232 OUTPUT → ADS1115 A0 (chân signal input)
AD8232 LO+    → ESP32 GPIO 13
AD8232 LO-    → ESP32 GPIO 14
AD8232 GND    → ESP32 GND
AD8232 VCC    → ESP32 3.3V
```

**⚠️ LƯU Ý:**

- AD8232 OUTPUT là analog signal (1.65V ± 0.5V)
- **KHÔNG** kết nối vào VDD hay GND!
- Dùng **cable ngắn < 10cm**

**Test bằng Multimeter:**

1. **Không gắn electrode:**
   - Đo giữa AD8232 OUTPUT và GND
   - Kết quả: ~1.65V (VCC/2)

2. **Gắn electrode, ngồi yên:**
   - Đo giữa AD8232 OUTPUT và GND
   - Kết quả: dao động 1.5V - 1.8V (theo nhịp tim)

3. **Nếu = 0V hoặc 3.3V:**
   - AD8232 hỏng hoặc wiring sai

---

#### **C. Electrode ECG Configuration:**

**3-Lead ECG (Lead I):**

```
        Head

   LA ●     ● RA
  (Y)       (R)


   ● RL
  (G)

Legend:
RA (Red):    Right Arm  → Cổ tay phải
LA (Yellow): Left Arm   → Cổ tay trái
RL (Green):  Right Leg  → Bụng hoặc mắt cá chân trái
```

**Vị trí tối ưu:**

- **Cổ tay phải (RA):** Mặt trong cổ tay, cách khớp 2-3cm
- **Cổ tay trái (LA):** Mặt trong cổ tay, cách khớp 2-3cm
- **Reference (RL):** Bụng dưới bên trái HOẶC mắt cá chân trái

**Chuẩn bị da:**

1. Lau sạch bằng cồn 70%
2. Đợi cồn khô hết (10 giây)
3. Cạo lông nếu có (để electrode dính tốt)
4. Dán electrode CHẶT

**Electrode quality:**

- ✅ Dùng electrode có GEL (medical grade)
- ✅ Kiểm tra hạn sử dụng (gel khô = không dẫn)
- ❌ KHÔNG dùng electrode tự chế từ nhôm/đồng

---

### BƯỚC 4: TEST TỪNG THÀNH PHẦN

#### **Test 1: ADS1115 riêng**

Tạo file `test_ads1115.cpp`:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);

  if (!ads.begin()) {
    Serial.println("ERROR: ADS1115 not found!");
    while(1);
  }

  ads.setGain(GAIN_TWO);
  ads.setDataRate(RATE_ADS1115_250SPS);

  Serial.println("ADS1115 OK! Reading A0...");
}

void loop() {
  int16_t adc = ads.readADC_SingleEnded(0);
  float voltage = adc * 0.0625;

  Serial.print("ADC:");
  Serial.print(adc);
  Serial.print(" Voltage:");
  Serial.print(voltage);
  Serial.println(" mV");

  delay(100);
}
```

**Kết quả mong đợi:**

- Không gắn gì vào A0: ADC gần 0, voltage gần 0
- Nối A0 vào 3.3V: ADC > 2000
- Nối A0 vào GND: ADC gần 0

---

#### **Test 2: AD8232 riêng**

Kết nối AD8232 OUTPUT → ESP32 GPIO 34 (ADC pin):

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(34, INPUT);
}

void loop() {
  int raw = analogRead(34);  // 0-4095
  float voltage = raw * (3.3 / 4095.0);

  Serial.print("Raw:");
  Serial.print(raw);
  Serial.print(" Voltage:");
  Serial.print(voltage);
  Serial.println(" V");

  delay(50);
}
```

**Kết quả mong đợi:**

- Không gắn electrode: ~2048 (1.65V)
- Gắn electrode: dao động 1800-2300 (1.4V-1.9V)

---

### BƯỚC 5: ĐO THỰC, PHÂN TÍCH KẾT QUẢ

#### **Tư thế đo chuẩn:**

1. **Ngồi thoải mái**
   - Lưng tựa ghế
   - Hai tay để thư giãn trên bàn
   - Không cử động, không nói chuyện

2. **Tắt nguồn nhiễu:**
   - Rút sạc laptop (dùng pin)
   - Tắt điện thoại hoặc để xa >50cm
   - Tắt đèn LED ceiling

3. **Thở đều:**
   - Hít vào 4 giây, thở ra 4 giây
   - Không nín thở

4. **Đợi tín hiệu ổn định:**
   - 30 giây đầu: signal có thể nhiễu
   - Sau 30 giây: signal ổn định

---

#### **Phân tích output:**

**✅ CHUẨN:**

```
Raw:1.23,Filtered:1.10,HR:72
Raw:1.45,Filtered:1.32,HR:72
Raw:0.98,Filtered:0.95,HR:72
Raw:2.10,Filtered:1.85,HR:73
```

→ Raw dao động ±1-2 mV, HR stable

**❌ SAI - Signal quá yếu:**

```
Raw:-0.19,Filtered:0.00,HR:0
Raw:0.00,Filtered:0.00,HR:0
```

→ Electrode không tiếp xúc tốt hoặc AD8232 chưa kết nối

**❌ SAI - Signal quá mạnh:**

```
Raw:50.23,Filtered:45.10,HR:0
Raw:48.56,Filtered:43.22,HR:0
```

→ Có thể kết nối sai (VCC vào A0)

**❌ SAI - Nhiễu 50Hz:**

```
Raw:1.23,Filtered:1.10,HR:72
Raw:1.56,Filtered:1.45,HR:153  ← Nhảy đột ngột
Raw:1.12,Filtered:1.05,HR:68
```

→ Nhiễu điện lưới, cần notch filter

---

## 🎯 EXPECTED VALUES (GIÁ TRỊ MONG ĐỢI)

### **Người trưởng thành khỏe mạnh:**

| Parameter        | Normal Range  | Your Value | Status          |
| ---------------- | ------------- | ---------- | --------------- |
| **Raw ECG**      | ±1 to ±3 mV   | -0.19 mV   | ❌ Quá yếu      |
| **Filtered ECG** | ±0.5 to ±2 mV | 0.00 mV    | ❌ Không có     |
| **Heart Rate**   | 60-100 BPM    | 0 BPM      | ❌ Không detect |
| **ADC Value**    | -500 to +500  | ???        | ⚠️ Cần check    |

---

## 🛠️ CHECKLIST SỬA LỖI:

### Hardware:

- [ ] ADS1115 VDD → ESP32 3.3V (KHÔNG phải 5V!)
- [ ] ADS1115 SDA → GPIO 21, SCL → GPIO 22
- [ ] ADS1115 ADDR → GND
- [ ] AD8232 OUTPUT → ADS1115 A0
- [ ] AD8232 LO+ → GPIO 13, LO- → GPIO 14
- [ ] Cable I2C ngắn < 20cm
- [ ] Cable AD8232→ADS1115 ngắn < 10cm

### Electrode:

- [ ] Electrode có gel, chưa hết hạn
- [ ] Da sạch, lau cồn
- [ ] Dán chặt, không nhăn
- [ ] Red → Cổ tay phải (inside wrist)
- [ ] Yellow → Cổ tay trái
- [ ] Green → Bụng hoặc mắt cá chân

### Software:

- [ ] Clean build: `pio run -e Main -t clean`
- [ ] Upload: `pio run -e Main -t upload`
- [ ] Serial Monitor: baudrate 115200
- [ ] Không còn `[E][Wire.cpp:513]` errors
- [ ] Thấy `[ADC_RAW]xxx` values

### Environment:

- [ ] Rút sạc laptop (dùng pin)
- [ ] Tắt điện thoại hoặc để xa
- [ ] Ngồi yên, thở đều
- [ ] Đợi 30s cho signal ổn định

---

## 📊 DEBUG TIMELINE:

### Minute 0-1: Upload & Basic Check

```
✓ Code uploaded
✓ Serial Monitor opened
✓ No Wire.cpp errors (after clean build)
✓ Seeing data stream
```

### Minute 1-2: ADC Value Check

```
[ADC_RAW]0 → AD8232 chưa kết nối
[ADC_RAW]1234 → Có signal, check electrode
[ADC_RAW]-456 → Có signal, ECG working
```

### Minute 2-3: Electrode Test

```
Attach electrodes → ADC value thay đổi
Breathe → ADC value dao động
Touch electrode → Spike trong signal
```

### Minute 3-5: Signal Quality

```
Raw: ±1-3 mV → Good
Filtered: ±0.5-2 mV → Good
HR: 60-100 BPM → Excellent!
```

---

## 🔄 NẾU VẪN KHÔNG HOẠT ĐỘNG:

### Option 1: Test với MAX30102 (pulse oximeter)

Nếu có MAX30102 trong project:

- Test HR bằng MAX30102
- So sánh với AD8232
- Verify ESP32 hoạt động tốt

### Option 2: Test với smartphone ECG app

- Download "ECG Viewer" app
- So sánh waveform
- Verify electrode placement

### Option 3: Simplify setup

```cpp
// Tắt hết filter, chỉ đọc raw ADC
int16_t adc = ads.readADC_SingleEnded(0);
Serial.println(adc);
```

- Xem ADC value có dao động không
- Nếu không → hardware issue
- Nếu có → filter issue

---

## 💡 TIPS NÂNG CAO:

### Tip 1: Tối ưu electrode placement

- Thay vì cổ tay: Thử upper arm (cánh tay trên)
- Distance RA-LA càng xa càng tốt (signal mạnh hơn)

### Tip 2: Giảm nhiễu

- Ngồi trên ghế gỗ (không dẫn điện)
- Chân không chạm sàn (nếu sàn bê tông)
- Wrap cable electrode bằng aluminum foil (shielding)

### Tip 3: Test với AA battery

- Thay nguồn USB bằng 2x AA battery (3V)
- Loại bỏ ground loop noise
- Signal sạch hơn nhiều

---

**Chúc bạn fix thành công! Nếu vẫn lỗi, gửi log `[ADC_RAW]xxx` để tôi phân tích tiếp.** 🚀
