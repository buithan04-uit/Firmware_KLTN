# AD8232 + ADS1115 - Sơ Đồ Kết Nối Nhanh

```
┌─────────────────────────────────────────────────────────────┐
│                    AD8232 ECG MODULE                         │
│                                                              │
│  ┌──────────────┐                                           │
│  │   AD8232     │                                           │
│  │   (Chip IC)  │                                           │
│  └──────────────┘                                           │
│                                                              │
│  LO+  LO-  OUTPUT  3.3V  GND                                │
│   │    │      │      │    │                                 │
└───┼────┼──────┼──────┼────┼─────────────────────────────────┘
    │    │      │      │    │
    │    │      │      │    │
    │    │      │      │    └──────┐
    │    │      │      └───────┐   │
    │    │      │              │   │
    │    │      └────────┐     │   │
    │    │               │     │   │
┌───┴────┴───────────────┼─────┼───┼─────────────────────────┐
│  GPIO  GPIO            │    3.3V GND                        │
│   13    14             │     │   │          ESP32           │
│                        │     │   │                          │
│                  SDA   SCL   │   │                          │
│                   │    │    │   │                          │
│              GPIO 21   22   │   │                          │
│                   │    │    │   │                          │
└───────────────────┼────┼────┼───┼─────────────────────────┘
                    │    │    │   │
                    │    │    │   │
       ┌────────────┴────┴────┴───┴─────────────┐
       │                                         │
       │         ADS1115 16-BIT ADC             │
       │                                         │
       │  VDD  GND  SCL  SDA  A0  A1  A2  A3   │
       │   │    │    │    │    │                │
       └───┼────┼────┼────┼────┼────────────────┘
           │    │    │    │    │
           │    │    │    │    │
          3.3V GND   │    │    └── (kết nối với OUTPUT của AD8232)
                     │    │
                 GPIO 22  GPIO 21
```

## Bảng Kết Nối Chi Tiết

### AD8232 ECG Module

| Chân AD8232 | Nối đến       | Ghi chú                |
| ----------- | ------------- | ---------------------- |
| OUTPUT      | ADS1115 A0    | Tín hiệu ECG analog    |
| LO+         | ESP32 GPIO 13 | Lead-off detection (+) |
| LO-         | ESP32 GPIO 14 | Lead-off detection (-) |
| 3.3V        | ESP32 3.3V    | Nguồn dương            |
| GND         | ESP32 GND     | Nguồn âm               |

### ADS1115 ADC Module

| Chân ADS1115 | Nối đến       | Ghi chú                       |
| ------------ | ------------- | ----------------------------- |
| VDD          | ESP32 3.3V    | Nguồn dương                   |
| GND          | ESP32 GND     | Nguồn âm                      |
| SCL          | ESP32 GPIO 22 | I2C Clock                     |
| SDA          | ESP32 GPIO 21 | I2C Data                      |
| A0           | AD8232 OUTPUT | Tín hiệu ECG vào              |
| A1, A2, A3   | Không dùng    | Có thể dùng cho cảm biến khác |

### Điện Cực ECG (3 dây - Màu Xanh, Đỏ, Vàng)

**Cách gắn theo màu dây chuẩn quốc tế:**

| Màu dây trên điện cực | Ký hiệu trên AD8232 | Vị trí gắn trên cơ thể                        |
| --------------------- | ------------------- | --------------------------------------------- |
| 🔴 **Đỏ**             | RA (Right Arm)      | Cổ tay phải hoặc ngực phải (dưới xương đòn)   |
| 🟡 **Vàng**           | LA (Left Arm)       | Cổ tay trái hoặc ngực trái (dưới xương đòn)   |
| 🟢 **Xanh**           | RL (Right Leg)      | Bụng dưới bên phải hoặc sườn phải (Reference) |

**Sơ đồ vị trí:**

```
    Ngực phải          Ngực trái
        🔴                🟡
       (Đỏ)             (Vàng)
        RA                LA


        Bụng/Sườn
            🟢
          (Xanh)
            RL
```

**💡 Mẹo để có tín hiệu tốt:**

- Lau sạch da bằng cồn 70% trước khi gắn điện cực
- Gắn điện cực chắc chắn, tránh lỏng lẻo
- Sử dụng gel dẫn điện ECG nếu có (tăng chất lượng tín hiệu)
- Tránh gắn trên xương, gắn trên vùng da có nhiều mô mềm
- Ngồi yên, thư giãn khi đo

## Lưu Ý Quan Trọng

### ⚡ Nguồn Điện

- **CHỈ dùng nguồn pin/USB** khi test
- **KHÔNG được** kết nối vào nguồn điện AC đồng thời
- Điện áp hoạt động: 3.3V (KHÔNG dùng 5V)

### 📏 Chất Lượng Tín Hiệu

- Dây kết nối giữa AD8232 và ADS1115 nên **ngắn** (<10cm)
- Dây I2C nên **ngắn** và tránh nhiễu
- Sử dụng dây chắn nhiễu nếu có thể
- Gắn điện cực ECG **chắc chắn** lên da

### 🔍 Kiểm Tra I2C

Nếu không phát hiện được ADS1115, kiểm tra địa chỉ I2C:

```cpp
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  Serial.println("Scanning I2C...");
  for (byte i = 0; i < 127; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(i, HEX);
    }
  }
}
```

- ADS1115 thường có địa chỉ: **0x48** (mặc định)

### 🛠️ Troubleshooting Nhanh

| Triệu chứng              | Nguyên nhân      | Giải pháp                        |
| ------------------------ | ---------------- | -------------------------------- |
| "Không tìm thấy ADS1115" | Lỗi I2C          | Kiểm tra SDA, SCL, nguồn         |
| HR = 0                   | Chưa có tín hiệu | Chờ 5-10 giây, kiểm tra điện cực |
| "Kiểm tra điện cực"      | Lead-off         | Gắn chặt điện cực, dùng gel      |
| Tín hiệu nhiễu quá       | Nhiễu điện       | Tránh xa nguồn AC, dùng pin      |
| Tín hiệu quá yếu         | Gain không đủ    | Kiểm tra kết nối OUTPUT -> A0    |

## Mẹo Tối Ưu

1. **Vệ sinh da**: Lau sạch vị trí gắn điện cực bằng cồn
2. **Dùng gel ECG**: Tăng độ dẫn điện, giảm nhiễu
3. **Ngồi yên**: Chuyển động sẽ tạo artifact
4. **Tránh căng cơ**: Cơ co rút tạo nhiễu EMG
5. **Để ổn định**: Chờ 10-20 giây sau khi gắn điện cực

## Kết Quả Mong Đợi

✅ Sau 5-10 giây: Hiển thị nhịp tim (60-100 BPM cho người nghỉ ngơi)  
✅ Tín hiệu ECG mượt, thấy rõ đỉnh R  
✅ Lead-off detection hoạt động ngay lập tức  
✅ Độ chính xác: ±2-3 BPM so với máy đo y tế

## Serial Monitor Output Mẫu

```
=== Khởi tạo AD8232 với ADS1115 ===
✓ ADS1115 khởi tạo thành công
✓ Bộ lọc nhiễu đã được thiết lập
✓ Tần số lấy mẫu: 250 Hz
========================================

=== AD8232 ECG Status ===
Leads Connected: YES
Raw Signal: 1250.35 mV
Filtered Signal: 0.82 mV
Heart Rate: 72 BPM
========================
```

---

**Lưu ý**: Thiết bị này chỉ dùng cho mục đích học tập, KHÔNG thay thế thiết bị y tế chuyên nghiệp!
