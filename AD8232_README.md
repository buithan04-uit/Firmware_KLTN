# AD8232 ECG với ADS1115 - Hướng Dẫn Sử Dụng

## Tính năng

✅ **ADC 16-bit chất lượng cao**: Sử dụng ADS1115 thay vì ADC built-in của ESP32  
✅ **Lọc nhiễu đa tầng**:

- Moving Average Filter (khử nhiễu tần số cao)
- Median Filter (loại bỏ spike đột biến)
- High-Pass Filter (loại bỏ baseline drift/DC offset)

✅ **Phát hiện nhịp tim real-time**: Tự động tính BPM  
✅ **Lead-off detection**: Phát hiện khi điện cực bị tuột  
✅ **Tần số lấy mẫu**: 250 Hz (chuẩn cho ECG)

---

## Kết nối phần cứng

### AD8232 ECG Module

```
AD8232          ESP32
-------         -----
OUTPUT    ->    (kết nối vào ADS1115 A0)
LO+       ->    GPIO 13
LO-       ->    GPIO 14
3.3V      ->    3.3V
GND       ->    GND
```

### ADS1115 ADC Module

```
ADS1115         ESP32
-------         -----
VDD       ->    3.3V
GND       ->    GND
SCL       ->    GPIO 22
SDA       ->    GPIO 21
A0        ->    AD8232 OUTPUT
```

### Gắn điện cực ECG (3 dây - Xanh, Đỏ, Vàng)

**Theo màu dây chuẩn quốc tế:**

| Màu dây | Ký hiệu trên AD8232      | Vị trí gắn trên cơ thể                      |
| ------- | ------------------------ | ------------------------------------------- |
| 🔴 Đỏ   | RA (Right Arm)           | Cổ tay phải hoặc ngực phải (dưới xương đòn) |
| 🟡 Vàng | LA (Left Arm)            | Cổ tay trái hoặc ngực trái (dưới xương đòn) |
| 🟢 Xanh | RL (Right Leg/Reference) | Bụng dưới bên phải hoặc mắt cá chân phải    |

**Vị trí tốt nhất để đo ECG:**

```
        Đỏ (RA)                     Vàng (LA)
           ●                           ●
    (Ngực phải, dưới xương đòn) (Ngực trái, dưới xương đòn)


              Xanh (RL)
                 ●
         (Sườn phải hoặc bụng)
```

💡 **Lưu ý:**

- Làm sạch da bằng cồn trước khi gắn
- Dùng gel ECG để giảm nhiễu (nếu có)
- Điện cực phải dính chặt vào da

---

## Cách sử dụng trong code

### 1. Thêm vào main.cpp

```cpp
#include <Arduino.h>
#include "ad8232.h"

void setup() {
  Serial.begin(115200);

  // Khởi tạo AD8232
  initAD8232();
}

void loop() {
  // Cập nhật đọc tín hiệu ECG
  updateAD8232();

  // Lấy dữ liệu
  if (areLeadsConnected()) {
    float rawSignal = getECGRawSignal();
    float filteredSignal = getECGFilteredSignal();
    int heartRate = getHeartRate();

    // Hiển thị lên màn hình hoặc gửi lên server
    Serial.print("HR: ");
    Serial.print(heartRate);
    Serial.println(" BPM");
  } else {
    Serial.println("Điện cực chưa được gắn đúng!");
  }

  // (Tùy chọn) In debug mỗi 500ms
  // printECGDebug();
}
```

### 2. Tích hợp với LVGL UI

```cpp
#include "ad8232.h"

// Tạo label hiển thị nhịp tim
lv_obj_t *labelHeartRate;

void setup() {
  // ... khởi tạo LVGL ...

  // Tạo UI element
  labelHeartRate = lv_label_create(lv_scr_act());
  lv_label_set_text(labelHeartRate, "-- BPM");

  // Khởi tạo AD8232
  initAD8232();
}

void loop() {
  updateAD8232();

  // Cập nhật UI mỗi 500ms
  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 500) {
    lastUIUpdate = millis();

    if (areLeadsConnected()) {
      int hr = getHeartRate();
      if (hr > 0) {
        String text = String(hr) + " BPM";
        lv_label_set_text(labelHeartRate, text.c_str());
      } else {
        lv_label_set_text(labelHeartRate, "Đang đo...");
      }
    } else {
      lv_label_set_text(labelHeartRate, "Kiểm tra điện cực");
    }
  }

  lv_timer_handler();
  delay(5);
}
```

### 3. Vẽ ECG Waveform với LVGL Chart

```cpp
#include "ad8232.h"

lv_obj_t *chart;
lv_chart_series_t *ser;

void setup() {
  // ... khởi tạo LVGL ...

  // Tạo chart
  chart = lv_chart_create(lv_scr_act());
  lv_obj_set_size(chart, 300, 150);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 100);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

  // Tạo series
  ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

  initAD8232();
}

void loop() {
  updateAD8232();

  // Cập nhật chart với tín hiệu đã lọc
  if (areLeadsConnected()) {
    float signal = getECGFilteredSignal();
    lv_chart_set_next_value(chart, ser, (int32_t)signal);
  }

  lv_timer_handler();
  delay(5);
}
```

### 4. Gửi dữ liệu lên Google Sheets

```cpp
void sendToGoogleSheets() {
  if (!areLeadsConnected()) return;

  int hr = getHeartRate();
  if (hr == 0) return; // Chưa có dữ liệu

  HTTPClient http;
  String url = GOOGLE_SCRIPT_URL + "?heartRate=" + String(hr);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.println("✓ Đã gửi HR: " + String(hr) + " BPM");
  } else {
    Serial.println("✗ Lỗi khi gửi dữ liệu");
  }

  http.end();
}

void loop() {
  updateAD8232();

  // Gửi mỗi 10 giây
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 10000) {
    lastSend = millis();
    sendToGoogleSheets();
  }
}
```

---

## API Reference

### Hàm khởi tạo

- `void initAD8232()` - Khởi tạo ADS1115, cấu hình I2C và bộ lọc

### Hàm cập nhật

- `void updateAD8232()` - Gọi trong loop() để đọc tín hiệu và phát hiện nhịp tim

### Hàm lấy dữ liệu

- `float getECGRawSignal()` - Tín hiệu thô từ ADC (mV)
- `float getECGFilteredSignal()` - Tín hiệu đã lọc nhiễu (mV)
- `int getHeartRate()` - Nhịp tim (BPM), trả về 0 nếu chưa phát hiện
- `bool areLeadsConnected()` - Kiểm tra trạng thái điện cực

### Hàm debug

- `void printECGDebug()` - In thông tin ra Serial (mỗi 500ms)

---

## Tham số có thể điều chỉnh

Trong file `ad8232.cpp`, bạn có thể điều chỉnh:

```cpp
#define MOVING_AVG_SIZE 4       // Kích thước bộ lọc trung bình (2-8)
#define MEDIAN_FILTER_SIZE 5    // Kích thước bộ lọc median (3-7, nên là số lẻ)
#define SAMPLE_RATE 250         // Tần số lấy mẫu Hz (200-500)
#define THRESHOLD_MULTIPLIER 1.5 // Ngưỡng phát hiện đỉnh R (1.2-2.0)
#define MIN_PEAK_DISTANCE 200   // Khoảng cách tối thiểu giữa 2 đỉnh (ms)
```

---

## Troubleshooting

### ❌ "ERROR: Không tìm thấy ADS1115!"

- Kiểm tra kết nối I2C (SDA, SCL)
- Kiểm tra nguồn 3.3V cho ADS1115
- Dùng I2C scanner để tìm địa chỉ (0x48)

### ❌ Heart Rate luôn = 0

- Kiểm tra điện cực đã gắn đúng chưa
- Chờ ít nhất 5 giây để hệ thống ổn định
- Điều chỉnh `THRESHOLD_MULTIPLIER` (tăng nếu quá nhạy, giảm nếu không phát hiện)

### ❌ Tín hiệu nhiễu quá nhiều

- Giảm `MOVING_AVG_SIZE` để lọc tốt hơn (nhưng sẽ giảm độ nhạy)
- Tăng `MEDIAN_FILTER_SIZE` để loại bỏ spike
- Kiểm tra kết nối mass của AD8232
- Đảm bảo dây điện cực không quá dài

### ❌ Baseline drift (tín hiệu trôi)

- High-pass filter đã được tích hợp để xử lý
- Có thể điều chỉnh `HPF_ALPHA` (0.95-0.99)

---

## Lưu ý quan trọng

⚠️ **Thiết bị này chỉ dùng cho mục đích học tập và nghiên cứu**  
⚠️ **KHÔNG sử dụng cho chẩn đoán y tế**  
⚠️ **KHÔNG kết nối vào nguồn điện khi đang đeo điện cực**  
⚠️ **Sử dụng nguồn pin để an toàn**

---

## Kết quả mong đợi

✅ Nhịp tim chính xác ±3 BPM  
✅ Tín hiệu ECG mượt mà, ít nhiễu  
✅ Phát hiện lead-off ngay lập tức  
✅ Latency thấp (~4ms với 250 Hz sampling)

---

## Nâng cấp tương lai

- [ ] Thêm Pan-Tompkins algorithm để phát hiện QRS complex
- [ ] Tính HRV (Heart Rate Variability)
- [ ] Phát hiện arrhythmia (rối loạn nhịp)
- [ ] Lưu dữ liệu vào SD card
- [ ] Gửi dữ liệu qua Bluetooth/WiFi real-time

---

**Tác giả**: ESP32 ECG Project  
**Phiên bản**: 1.0  
**Ngày cập nhật**: 2025
