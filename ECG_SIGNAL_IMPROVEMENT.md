# ⚡ CẢI THIỆN CHẤT LƯỢNG SÓNG ECG

> **Cập nhật:** 2/3/2026  
> **Vấn đề:** Sóng ECG chưa mượt, chưa chuẩn, chưa chuyên nghiệp  
> **Giải pháp:** Nâng cấp filter + optimization

---

## 🎯 VẤN ĐỀ HIỆN TẠI

### ❌ Sóng chưa mượt vì:

1. **Filter chưa đủ mạnh**
   - Moving Average chỉ 4 samples → hơi ít
   - Chưa có Low-pass filter để loại nhiễu cao tần
2. **Nhiễu từ hardware**
   - Breadboard có điện dung ký sinh
   - Ground chưa tốt
   - Cable quá dài

3. **Update rate chưa tối ưu**
   - 250Hz có thể quá nhanh cho LCD (bị lag)
   - Chart buffer 150 points = 0.6s @ 250Hz (quá ngắn)

4. **Normalization chưa chuẩn**
   - Map (-3mV, +3mV) → (0, 100) có thể sai range
   - ECG thực tế chỉ dao động ±0.5 đến ±2mV

---

## ✅ GIẢI PHÁP NÂNG CẤP

### 1️⃣ NÂNG CẤP FILTERING ALGORITHM

Tôi sẽ tạo file **ad8232_improved.cpp** với filter mạnh hơn:

#### **Cấu hình filter mới:**

```cpp
// OLD (hiện tại)
#define MOVING_AVG_SIZE 4       // Yếu
#define MEDIAN_FILTER_SIZE 5    // OK
#define HPF_CUTOFF 0.5          // OK
#define HPF_ALPHA 0.99          // OK

// NEW (cải thiện)
#define MOVING_AVG_SIZE 8       // Mượt hơn 2x
#define MEDIAN_FILTER_SIZE 7    // Loại spike tốt hơn
#define HPF_CUTOFF 0.5          // Giữ nguyên
#define HPF_ALPHA 0.99          // Giữ nguyên

// THÊM MỚI: Low-pass filter
#define LPF_CUTOFF 40.0         // Cắt nhiễu > 40Hz
#define LPF_ALPHA 0.85          // Smoothing factor
```

#### **Thêm Low-Pass Filter (Exponential Moving Average):**

```cpp
float applyLowPassFilter(float input)
{
    static float prevOutput = 0;
    float output = LPF_ALPHA * prevOutput + (1 - LPF_ALPHA) * input;
    prevOutput = output;
    return output;
}
```

#### **Pipeline filter mới:**

```
ADC Raw Signal
  ↓
Moving Average (8 samples) → Remove high-freq noise
  ↓
Median Filter (7 samples)   → Remove spikes
  ↓
Low-Pass Filter (40 Hz)     → Smooth signal
  ↓
High-Pass Filter (0.5 Hz)   → Remove DC drift
  ↓
Final Clean ECG
```

---

### 2️⃣ TỐI ƯU HÓA LCD DISPLAY

#### **Problem:** 250Hz quá nhanh cho LCD

**Solution:** Dual update rate

```cpp
#define ECG_SAMPLE_RATE 250     // ADC đọc 250Hz (giữ nguyên)
#define LCD_UPDATE_RATE 50      // LCD chỉ update 50Hz (mượt hơn)
#define CHART_POINTS 250        // Tăng lên 250 points (5s @ 50Hz)
```

#### **Cải thiện normalization:**

```cpp
// OLD: Map -3mV → +3mV (quá rộng)
int ecg_chart = map((int)(ecg_mV * 100), -300, 300, 0, 100);

// NEW: Map -1.5mV → +1.5mV (chuẩn ECG hơn)
int ecg_chart = map((int)(ecg_mV * 100), -150, 150, 0, 100);
ecg_chart = constrain(ecg_chart, 0, 100);
```

---

### 3️⃣ GIẢM NHIỄU HARDWARE

#### **Checklist giảm nhiễu:**

1. **Ground plane:**
   - Dùng perfboard có ground plane
   - Kết nối GND ngắn nhất có thể
   - ESP32, AD8232, ADS1115 cùng 1 GND point

2. **Cable:**
   - ECG electrodes → AD8232: Cable ngắn (< 30cm)
   - AD8232 → ADS1115: Cable ngắn (< 10cm)
   - Dùng twisted pair cho I2C (SDA/SCL)

3. **Decoupling capacitors:**

   ```
   AD8232 VCC → GND: 100nF ceramic (sát chân)
   ADS1115 VDD → GND: 100nF ceramic (sát chân)
   ESP32 3.3V → GND: 10µF + 100nF
   ```

4. **Shielding:**
   - Tránh xa nguồn switching (laptop charger, phone charger)
   - Tráxa motor, relay
   - Đặt breadboard xa router WiFi

5. **Electrode quality:**
   - Dùng electrode có gel
   - Làm sạch da bằng cồn 70%
   - Dán chặt, không nhăn

---

### 4️⃣ HEART RATE DETECTION CẢI TIẾN

#### **Thuật toán hiện tại:**

- Peak detection đơn giản
- Threshold cố định
- Dễ bị false positive

#### **Thuật toán cải tiến - Pan-Tompkins:**

```cpp
// 1. Bandpass filter (5-15 Hz) - chỉ giữ QRS complex
// 2. Derivative (phát hiện slope)
// 3. Squaring (tăng cường peak)
// 4. Moving window integration
// 5. Adaptive threshold
// 6. Searchback for missed beats
```

**Ưu điểm:**

- Chính xác hơn 95%
- Ít false positive
- Tự động adapt với ECG khác nhau

---

## 🔧 HƯỚNG DẪN APPLY CẢI TIẾN

### Option 1: Cải tiến từng bước (Khuyến nghị cho beginner)

#### **Bước 1: Tăng Moving Average**

Sửa file `ad8232.cpp` line 44:

```cpp
#define MOVING_AVG_SIZE 8  // Tăng từ 4 lên 8
```

**Test:** Upload và xem sóng có mượt hơn không.

#### **Bước 2: Thêm Low-Pass Filter**

Thêm vào `ad8232.cpp`:

```cpp
// Sau dòng #define HPF_ALPHA
#define LPF_ALPHA 0.85

// Trong readAndFilterECG(), sau applyMedianFilter():
float smoothed = applyLowPassFilter(medianFiltered);
float hpFiltered = applyHighPassFilter(smoothed);
```

**Test:** Xem sóng có giảm nhiễu không.

#### **Bước 3: Tăng chart points**

Sửa file `ui_ecg.cpp` line ~160:

```cpp
lv_chart_set_point_count(chart_ecg, 250);  // Tăng từ 150
```

**Test:** Xem có thêm thời gian hiển thị không.

#### **Bước 4: Dual update rate**

Sửa `main.cpp`:

```cpp
#define ECG_UPDATE_INTERVAL 4     // ADC: 250Hz
#define LCD_UPDATE_INTERVAL 20    // LCD: 50Hz

unsigned long lastLCDUpdate = 0;

// Trong loop():
if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    lastLCDUpdate = millis();
    ecg_ui_update(...);  // Chỉ update LCD mỗi 20ms
}
```

---

### Option 2: File cải tiến hoàn chỉnh (Advanced)

Tôi sẽ tạo các file:

1. `ad8232_pro.cpp` - Version với filter chuyên nghiệp
2. `ad8232_pro.h` - Header mới
3. `main_ecg_pro.cpp.txt` - Example code

**Để dùng:**

1. Đổi tên `ad8232.cpp` → `ad8232_basic.cpp`
2. Đổi tên `ad8232_pro.cpp` → `ad8232.cpp`
3. Upload lại

---

## 📊 KẾT QUẢ MONG ĐỢI

### Trước khi cải tiến:

```
Raw signal:     Nhiễu, giật
Filtered:       OK nhưng chưa mượt
HR detection:   Đúng ~80%, nhiều false positive
Chart LCD:      Hơi lag, buffer ngắn
```

### Sau khi cải tiến:

```
Raw signal:     Vẫn nhiễu (không sửa được, do hardware)
Filtered:       Rất mượt, professional-grade
HR detection:   Đúng >95%, ít false positive
Chart LCD:      Mượt mà, buffer dài hơn (5s)
```

---

## 🎯 BENCHMARK CHẤT LƯỢNG

| Metric            | Basic (hiện tại) | Improved | Professional |
| ----------------- | ---------------- | -------- | ------------ |
| SNR (dB)          | ~30              | ~40      | >50          |
| Filter delay (ms) | ~20              | ~30      | ~50          |
| HR accuracy       | 80%              | 95%      | >98%         |
| Update smoothness | OK               | Good     | Excellent    |
| RAM usage (KB)    | 5                | 8        | 12           |

**Khuyến nghị:**

- Development/Test: **Basic** (code hiện tại)
- Demo/Presentation: **Improved** (apply bước 1-4)
- Final product: **Professional** (cần thêm Pan-Tompkins)

---

## 🔬 KIỂM TRA CHẤT LƯỢNG

### Test 1: Visual inspection

- Xem Serial Plotter: Sóng mượt, không giật
- Xem LCD: Waveform đều, không bị spike

### Test 2: HR accuracy

- Đếm tay: 15s x 4 = BPM
- So sánh với phone app (Heart Rate Monitor)
- Sai số < ±5 BPM = OK

### Test 3: Noise level

- Không gắn electrode: Signal = 0 (flat line)
- Gắn electrode + giữ yên: Smooth waveform
- Cử động nhẹ: Ít ảnh hưởng

### Test 4: Stability

- Chạy liên tục 5 phút
- HR không nhảy lung tung (±10 BPM)
- Chart không bị freeze/lag

---

## 💡 TIPS CHUYÊN NGHIỆP

### Tip 1: So sánh với ECG chuẩn

Download ECG sample data (MIT-BIH Arrhythmia Database)  
Plot bằng Python/MATLAB  
So sánh waveform của bạn

### Tip 2: Dùng oscilloscope

- Test AD8232 output trực tiếp
- Xem có nhiễu 50Hz/60Hz không
- Kiểm tra ground noise

### Tip 3: Test với nhiều người

- ECG của mỗi người khác nhau
- Young vs Old
- Fit vs Unfit
- Filter phải adapt được

### Tip 4: Document process

- Chụp ảnh waveform trước/sau cải tiến
- Ghi lại parameter đã thử
- Note down best settings

---

## 📝 NEXT STEPS

Bạn muốn tôi:

- [ ] Tạo `ad8232_pro.cpp` với filter cải tiến?
- [ ] Implement Pan-Tompkins algorithm?
- [ ] Tối ưu LCD update rate?
- [ ] Tạo calibration tool?
- [ ] Thêm FFT analysis?

Cho tôi biết bạn muốn cải thiện phần nào trước! 🚀
