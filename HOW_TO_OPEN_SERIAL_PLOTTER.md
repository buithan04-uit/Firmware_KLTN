# 🚀 QUICK START - MỞ SERIAL PLOTTER

> **Cập nhật:** 2/3/2026  
> **Mục đích:** Hướng dẫn mở Serial Plotter để xem ECG realtime

---

## 📊 CÁCH 1: PLATFORMIO SERIAL MONITOR (Khuyến nghị)

### Bước 1: Upload code

```bash
pio run -e Main -t upload
```

### Bước 2: Mở Serial Monitor

Click vào icon **🔌 Serial Monitor** ở thanh dưới PlatformIO

**Hoặc:**

- Menu: `PlatformIO` → `Monitor`
- Shortcut: `Ctrl + Alt + S`

### Bước 3: Xem output

Bạn sẽ thấy:

```
========== ECG DATA STREAM ==========
Raw:125.34 Filtered:123.12 HR:72
Raw:126.01 Filtered:123.45 HR:72
Raw:124.88 Filtered:122.98 HR:72
...
```

**Lưu ý:** PlatformIO Serial Monitor hiển thị TEXT, không tự động vẽ đồ thị.  
Để xem đồ thị, dùng **Arduino IDE Serial Plotter** (Cách 2) hoặc **TeleplotG** (Cách 3).

---

## 📈 CÁCH 2: ARDUINO IDE SERIAL PLOTTER (Có đồ thị)

### Yêu cầu:

- Đã cài Arduino IDE 2.x

### Bước 1: Kết nối ESP32 qua USB

### Bước 2: Mở Arduino IDE

### Bước 3: Chọn COM Port

`Tools` → `Port` → Chọn COM port của ESP32 (ví dụ: COM3)

### Bước 4: Set Baudrate

`Tools` → `Serial Monitor Settings` → Baudrate: **115200**

### Bước 5: Mở Serial Plotter

`Tools` → `Serial Plotter`

**Hoặc:**
Shortcut: `Ctrl + Shift + L`

### Bước 6: Xem đồ thị realtime

Bạn sẽ thấy 3 đường:

- **Raw** (màu đỏ): Tín hiệu thô
- **Filtered** (màu xanh): Tín hiệu sau khi lọc
- **HR** (màu vàng): Heart rate (BPM)

**Lưu ý:**

- Đồ thị tự động scroll từ phải sang trái
- Y-axis tự động scale
- Có thể pause bằng nút ⏸️

---

## 🌐 CÁCH 3: TELEPLOTG (Advanced - Đẹp nhất)

### Bước 1: Cài đặt TeleplotG

1. Mở **Chrome/Edge** browser
2. Vào Chrome Web Store
3. Tìm "**TeleplotG**"
4. Click **Add to Chrome**

Link: https://chrome.google.com/webstore/detail/teleplotg

### Bước 2: Mở TeleplotG

Click icon **TeleplotG** trên thanh extension

### Bước 3: Connect Serial Port

1. Click **Connect Serial**
2. Chọn COM port của ESP32
3. Baudrate: **115200**
4. Click **Connect**

### Bước 4: Xem đồ thị

TeleplotG tự động parse format `Label:Value` và vẽ đồ thị.

**Features:**

- ✅ Multiple charts
- ✅ Zoom in/out
- ✅ Pause/Resume
- ✅ Export data to CSV
- ✅ Change colors
- ✅ Auto-scaling

---

## 🐛 TROUBLESHOOTING

### ❌ Không thấy data trong Serial Monitor

**Nguyên nhân:**

- ESP32 chưa upload code
- Chọn sai COM port
- Baudrate sai

**Giải pháp:**

1. Upload code lại: `pio run -e Main -t upload`
2. Kiểm tra Device Manager → Ports (COM & LPT)
3. Đảm bảo baudrate = 115200
4. Reset ESP32 (nhấn nút EN)

---

### ❌ Serial Plotter không vẽ đồ thị

**Nguyên nhân:**

- Format output không đúng
- Baudrate sai
- Data không continuous

**Giải pháp:**

1. Kiểm tra format: `Raw:xxx Filtered:xxx HR:xxx`
2. Đảm bảo có space giữa các cặp Label:Value
3. Kiểm tra baudrate: 115200
4. Reset ESP32 và reconnect

**Kiểm tra format trong code:**

```cpp
Serial.print("Raw:");
Serial.print(raw, 2);
Serial.print(" Filtered:");  // ← Phải có SPACE ở đầu
Serial.print(filtered, 2);
Serial.print(" HR:");         // ← Phải có SPACE ở đầu
Serial.println(hr);
```

---

### ❌ Đồ thị có nhưng không mượt/bị giật

**Nguyên nhân:**

- Update rate thấp
- Serial buffer overflow
- USB connection không ổn định

**Giải pháp:**

1. **Tăng baudrate:**

   ```cpp
   Serial.begin(230400); // Hoặc 500000
   ```

2. **Giảm debug messages:**
   Comment phần debug info trong loop()

3. **Dùng USB cable ngắn, chất lượng tốt**

4. **Tắt các chương trình khác dùng COM port**

---

### ❌ LCD không hiển thị gì

**Nguyên nhân:**

- TFT_eSPI config sai
- Wiring sai
- LCD chưa có nguồn

**Giải pháp:**

1. **Kiểm tra mode:** Đảm bảo `ECG_MODE_DUAL` hoặc `ECG_MODE_LCD_DISPLAY` được bật
2. **Kiểm tra wiring:**
   ```
   TFT MOSI → GPIO 23
   TFT SCK  → GPIO 18
   TFT CS   → GPIO 15
   TFT DC   → GPIO 16
   TFT RST  → GPIO 17
   TFT VCC  → 3.3V hoặc 5V
   TFT GND  → GND
   ```
3. **Kiểm tra Serial Monitor:** Phải thấy:
   ```
   🎨 Initializing LCD...
   🎨 Initializing ECG UI...
   ✓ LCD ready
   ```
4. **Test LCD riêng:** Upload example TFT_eSPI (graphicstest)

---

## 📝 CHECKLIST KHI MỞ PLOTTER

- [ ] Code đã upload thành công (Exit Code: 0)
- [ ] Đã chọn đúng COM port
- [ ] Baudrate = 115200
- [ ] Serial Monitor/Plotter đã mở
- [ ] Thấy "ECG DATA STREAM" trong Serial
- [ ] Điện cực đã gắn (3 dây: Red, Yellow, Green)

---

## 🎯 OUTPUT MẪU CHUẨN

**Khi điện cực chưa gắn:**

```
⚠️  WARNING: Leads not connected!
   Check RED, YELLOW, GREEN electrodes

Raw:0.00 Filtered:0.00 HR:0
Raw:0.00 Filtered:0.00 HR:0
```

**Khi điện cực đã gắn đúng:**

```
Raw:125.34 Filtered:123.12 HR:72
Raw:126.01 Filtered:123.45 HR:72
Raw:124.88 Filtered:122.98 HR:73
Raw:125.67 Filtered:123.90 HR:72
...
```

**Debug info (mỗi 1 giây):**

```
[DEBUG] HR:72 | ECG:1.23 mV | Leads:OK
```

---

## 💡 TIPS

### Tip 1: Tối ưu Serial Plotter

- Chỉ plot **Filtered** và **HR** (comment dòng Raw để plot sạch hơn)
- Giảm decimal places: `Serial.print(value, 1)` thay vì `.print(value, 2)`

### Tip 2: Record data để phân tích sau

**PlatformIO:**

```bash
pio device monitor > ecg_data.txt
```

**Arduino IDE:**
Copy/Paste từ Serial Monitor vào file .txt

### Tip 3: So sánh Serial Plotter vs LCD

Chế độ DUAL cho phép xem 2 output cùng lúc:

- PC: Màn hình lớn, dễ phân tích
- LCD: Portable, real device experience

---

## 📊 HÌNH ẢNH MINH HỌA

**Arduino Serial Plotter:**

```
╭─────────────────────────────────────╮
│  150│     ╱╲    ╱╲    ╱╲           │
│  125│    ╱  ╲  ╱  ╲  ╱  ╲          │
│  100│   ╱    ╲╱    ╲╱    ╲         │
│   75│  ╱                   ╲        │ ← Filtered (xanh)
│   50│ ╱                     ╲       │
│   25│╱                       ╲      │
│    0├─────────────────────────────► │
│     │  Raw: 125  Filtered: 123     │
│     │  HR: 72                       │
╰─────────────────────────────────────╯
```

**TeleplotG:**

- Multiple charts, zoom, pan
- Export CSV cho MATLAB/Python
- Dark theme, customizable colors

---

**Chúc bạn xem ECG thành công! 🚀**

Nếu vẫn gặp vấn đề, kiểm tra [ECG_USER_GUIDE.md](ECG_USER_GUIDE.md) hoặc [BREADBOARD_TESTING_CHECKLIST.md](BREADBOARD_TESTING_CHECKLIST.md).
