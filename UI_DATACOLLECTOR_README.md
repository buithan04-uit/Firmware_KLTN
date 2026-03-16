# UI LVGL DATA COLLECTOR - HƯỚNG DẪN SỬ DỤNG

## 📋 Tổng quan

Bộ giao diện LVGL chuyên nghiệp cho Data Collector với:

- ✅ 2 màn hình: **Monitor** (hiển thị dữ liệu) & **Settings** (cài đặt)
- ✅ Real-time hiển thị khoảng cách & nhiệt độ
- ✅ Progress bar khi đo
- ✅ Popup thông báo đẹp mắt
- ✅ Thiết kế tương tự Medical Monitor UI

---

## 📁 Cấu trúc Files

```
PlatformIO/ESP32/
├── include/
│   └── ui_datacollector.h          # Header file - khai báo functions
├── src/
│   ├── ui_datacollector.cpp        # Implementation - code UI
│   ├── main_lvgl_example.cpp.txt   # Code mẫu tích hợp
│   └── main.cpp                    # File chính hiện tại
└── lib/
```

---

## 🎨 Thiết kế UI

### **Monitor Screen** (Màn hình chính)

```
┌─────────────────────────────────────┐
│ DATA COLLECTOR v2.0            W    │ ← Header (Navy)
├──────────────────┬─────────────────┤
│ ID: [1]          │ MAU: [0/5]      │ ← Info panels
├──────────────────┼─────────────────┤
│ KHOANG CACH      │ NHIET DO DA     │
│                  │                 │
│     [45] mm      │    [36.5] C     │ ← Data panels
│                  │                 │
├──────────────────┴─────────────────┤
│ [ENTER] Lay & Gui                  │
│ [<] Truoc   [>] Tiep               │ ← Footer
│ Dieu chinh khoang cach roi ENTER   │
└─────────────────────────────────────┘
```

### **Settings Screen** (Màn hình cài đặt)

```
┌─────────────────────────────────────┐
│           CAI DAT                   │ ← Header
├─────────────────────────────────────┤
│ WiFi: Connected                     │
│ SSID: C                             │ ← WiFi panel
├─────────────────────────────────────┤
│ ID NGUOI DO                         │
│         [-]   [1]   [+]             │ ← ID setting
├─────────────────────────────────────┤
│ SO MAU DA LAY: 0/5        [RESET]   │ ← Sample count
├─────────────────────────────────────┤
│         [← TRO VE]                  │ ← Back button
└─────────────────────────────────────┘
```

---

## 🔧 Cách tích hợp vào main.cpp

### **Option 1: Thay thế hoàn toàn TFT_eSPI**

1. **Backup main.cpp hiện tại**
2. **Copy nội dung từ** `main_lvgl_example.cpp.txt`
3. **Paste vào main.cpp**
4. **Upload**

### **Option 2: Giữ cả 2 UI (switch bằng #define)**

Thêm vào đầu `main.cpp`:

```cpp
#define USE_LVGL_UI  // Bỏ comment dòng này để dùng LVGL UI

#ifdef USE_LVGL_UI
  #include "ui_datacollector.h"
  // ... LVGL code ...
#else
  // ... TFT_eSPI code hiện tại ...
#endif
```

---

## 📖 API Functions

### **Khởi tạo UI**

```cpp
void ui_datacollector_init(void);
```

Gọi **1 lần** trong `setup()` sau khi init LVGL.

### **Cập nhật dữ liệu sensors**

```cpp
void ui_datacollector_update_sensors(float distance, float temperature);
```

Gọi trong `loop()` để update real-time.

### **Cập nhật ID người đo**

```cpp
void ui_datacollector_update_id(int id);
```

### **Cập nhật progress**

```cpp
void ui_datacollector_update_progress(int current, int total);
```

### **Cập nhật WiFi status**

```cpp
void ui_datacollector_update_wifi(bool connected);
```

### **Hiển thị popup**

```cpp
void ui_datacollector_show_popup(
  const char *title,        // Tiêu đề
  const char *message,      // Nội dung
  lv_color_t color,         // Màu nền (dùng lv_palette_main)
  bool show_progress        // Có progress bar không
);
```

**Ví dụ:**

```cpp
// Success popup
ui_datacollector_show_popup("THANH CONG!", "Du lieu da gui!",
                             lv_palette_main(LV_PALETTE_GREEN), false);

// Progress popup
ui_datacollector_show_popup("DANG LAY MAU", "Lay 20 mau...",
                             lv_palette_main(LV_PALETTE_BLUE), true);
```

### **Ẩn popup**

```cpp
void ui_datacollector_hide_popup(void);
```

### **Update progress bar trong popup**

```cpp
void ui_datacollector_update_popup_progress(int percentage); // 0-100
```

### **Chuyển màn hình**

```cpp
void ui_datacollector_load_monitor(void);   // Về Monitor
void ui_datacollector_load_settings(void);  // Vào Settings
```

---

## 🎮 Event Handlers (cần implement)

Các hàm này **BẮT BUỘC** phải có trong `main.cpp`:

```cpp
void ui_event_take_measurement(lv_event_t *e);  // ENTER - Đo & gửi
void ui_event_id_minus(lv_event_t *e);          // Settings: ID -
void ui_event_id_plus(lv_event_t *e);           // Settings: ID +
void ui_event_sample_reset(lv_event_t *e);      // Settings: Reset count
void ui_event_back_to_monitor(lv_event_t *e);   // Settings: Back
```

Xem code mẫu trong `main_lvgl_example.cpp.txt`.

---

## ⚙️ Setup LVGL

### **1. Khởi tạo trong setup()**

```cpp
void setup() {
  // ... TFT init ...
  tft.begin();
  tft.setRotation(1);

  // LVGL init
  lv_init();

  static lv_disp_draw_buf_t draw_buf;
  static lv_color_t buf[320 * 10];
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Tạo UI
  ui_datacollector_init();
  ui_datacollector_update_wifi(WiFi.status() == WL_CONNECTED);
}
```

### **2. Flush callback**

```cpp
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}
```

### **3. Loop**

```cpp
void loop() {
  lv_task_handler(); // QUAN TRỌNG - Gọi mỗi loop!

  // Update sensors
  if (millis() - lastUpdate > 250) {
    ui_datacollector_update_sensors(distance, temperature);
    lastUpdate = millis();
  }

  // Button handling...

  delay(5);
}
```

---

## 🎯 Workflow đo đạc

1. **Màn Monitor**: Hiển thị real-time distance & temp
2. **Nhấn ENTER**:
   - Popup "DANG LAY MAU" + progress bar
   - Lấy 20 mẫu, progress 0→100%
   - Popup "KET QUA DO" + giá trị
   - Popup "GUI DU LIEU"
   - Popup "THANH CONG!" hoặc "LOI"
3. **Nhấn LEFT/RIGHT**: Thay đổi ID (nguời trước/tiếp)
4. **Giữ UP 1s**: Vào Settings screen
5. **Settings screen**:
   - Xem WiFi status
   - Thay đổi ID (+/-)
   - Reset sample count
   - Back về Monitor

---

## 🎨 Màu sắc

```cpp
// Popup colors
lv_palette_main(LV_PALETTE_BLUE)   // Màu xanh dương (processing)
lv_palette_main(LV_PALETTE_GREEN)  // Màu xanh lá (success)
lv_palette_main(LV_PALETTE_RED)    // Màu đỏ (error)
lv_palette_main(LV_PALETTE_ORANGE) // Màu cam (warning)
```

---

## 📦 Dependencies

- **TFT_eSPI** (display driver)
- **LVGL** (UI library) - đã có sẵn từ Medical UI
- **Adafruit_MLX90614** (temperature sensor)
- **Adafruit_VL53L0X** (distance sensor)

---

## 🔍 Troubleshooting

### **Lỗi: màn hình trắng**

- Kiểm tra `lv_task_handler()` có được gọi trong `loop()`
- Kiểm tra `my_disp_flush()` callback

### **Lỗi: text bị cắt**

- Font size quá lớn → giảm size hoặc tăng panel width

### **Lỗi: UI chậm**

- Giảm buffer size: `320 * 10` → `320 * 5`
- Tăng delay trong loop: `delay(5)` → `delay(10)`

### **Lỗi: compile "undefined reference"**

- Thêm event handler functions vào `main.cpp`

---

## 💡 Ví dụ sử dụng

**Update sensors:**

```cpp
void loop() {
  lv_task_handler();

  float dist = getCorrectedDistance(rawDist);
  float temp = mlx.readObjectTempC();
  ui_datacollector_update_sensors(dist, temp);
}
```

**Đo mẫu với popup:**

```cpp
void measureSample() {
  ui_datacollector_show_popup("LAY MAU", "Dang do...",
                               lv_palette_main(LV_PALETTE_BLUE), true);

  for (int i = 0; i < 20; i++) {
    // ... do measurement ...
    ui_datacollector_update_popup_progress(i * 5);
    lv_task_handler();
  }

  ui_datacollector_show_popup("XONG!", "Thanh cong!",
                               lv_palette_main(LV_PALETTE_GREEN), false);
  delay(1500);
  ui_datacollector_hide_popup();
}
```

---

## 📝 Notes

- UI này **độc lập** với `ui.cpp` (Medical UI)
- Có thể dùng **cùng lúc** cả 2 UI trong project
- Fonts mặc định: Montserrat (10, 12, 14, 16, 18, 20, 24, 48)
- Resolution: 320x240 (landscape)

---

## 🚀 Quick Start

1. File đã sẵn sàng - không cần sửa gì thêm
2. Copy code từ `main_lvgl_example.cpp.txt` vào `main.cpp`
3. Upload
4. Test buttons:
   - ENTER: Đo & gửi
   - LEFT/RIGHT: Thay ID
   - UP (giữ 1s): Vào Settings

---

**Created by:** GitHub Copilot  
**Version:** 2.0  
**Date:** February 28, 2026
