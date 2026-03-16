/*
 * ===================================================================
 * ECG MONITOR - AD8232 + ADS1115
 * ===================================================================
 *
 * CHẾ ĐỘ HOẠT ĐỘNG:
 *
 * MODE 1: SERIAL PLOTTER
 *   - Chỉ Serial Plotter (không LCD)
 *   - Nhẹ, tiết kiệm RAM
 *   - Dùng để: Test nhanh khi chưa có LCD
 *
 * MODE 2: LCD DISPLAY
 *   - Chỉ LCD (không Serial Plotter)
 *   - Standalone device
 *   - Dùng để: Demo, presentation
 *
 * MODE 3: DUAL MODE (Khuyến nghị cho debug)
 *   - Chạy SONG SONG cả Serial Plotter VÀ LCD
 *   - So sánh realtime giữa PC và LCD
 *   - Dùng để: Verify, debug, so sánh tín hiệu
 *
 * CÁCH CHUYỂN MODE:
 *   - Comment/Uncomment #define ECG_MODE_xxx ở dưới
 *   - Chỉ được BẬT 1 MODE tại 1 thời điểm
 */

// ==========================================
// CHỌN MODE (CHỈ BẬT 1 DÒNG)
// ==========================================
// #define ECG_MODE_SERIAL_PLOTTER  // Mode 1: Chỉ Serial Plotter
// #define ECG_MODE_LCD_DISPLAY     // Mode 2: Chỉ LCD
#define ECG_MODE_DUAL // Mode 3: Song song Serial + LCD

// ==========================================
// INCLUDES
// ==========================================
#include <Arduino.h>
#include "ad8232.h"

#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
#include <TFT_eSPI.h>
#include "ui_ecg.h"
TFT_eSPI tft = TFT_eSPI();
#endif

// ==========================================
// TIMING CONFIG
// ==========================================
#define ECG_UPDATE_INTERVAL 4 // 250Hz (4ms)
#define DEBUG_INTERVAL 1000   // Debug info mỗi 1 giây

#ifdef ECG_MODE_SERIAL_PLOTTER
#define SERIAL_OUTPUT_INTERVAL 4 // Plotter-only: xuất đủ 250Hz
#else
#define SERIAL_OUTPUT_INTERVAL 8 // Dual/LCD: 125Hz dễ đọc hơn, vẫn giữ đỉnh bằng peak-hold
#endif

#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
#define LVGL_TICK_INTERVAL 5 // LVGL handler 200Hz
#endif

// ==========================================
// GLOBAL VARIABLES
// ==========================================
unsigned long lastECGUpdate = 0;
unsigned long lastDebugTime = 0;

#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
unsigned long lastLVGLTick = 0;
#endif

// ==========================================
// SETUP
// ==========================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // ========== HEADER ==========
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║     ECG MONITOR - AD8232 + ADS1115               ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();

// ========== DETECT MODE ==========
#ifdef ECG_MODE_SERIAL_PLOTTER
  Serial.println("📊 MODE: SERIAL PLOTTER ONLY");
  Serial.println("   Output: Serial Monitor/Plotter");
  Serial.println("   Format: Raw:xxx Filtered:xxx HR:xxx");
  Serial.println();
#elif defined(ECG_MODE_LCD_DISPLAY)
  Serial.println("🖥️  MODE: LCD DISPLAY ONLY");
  Serial.println("   Output: TFT LCD Screen");
  Serial.println("   UI: Standalone ECG interface");
  Serial.println();
#elif defined(ECG_MODE_DUAL)
  Serial.println("🔄 MODE: DUAL (Serial + LCD)");
  Serial.println("   Output 1: Serial Monitor/Plotter");
  Serial.println("   Output 2: TFT LCD Screen");
  Serial.println("   Purpose: Compare & Verify signals");
  Serial.println();
#else
#error "No mode selected! Define ECG_MODE_SERIAL_PLOTTER, ECG_MODE_LCD_DISPLAY, or ECG_MODE_DUAL"
#endif

  // ========== ELECTRODE CONNECTION GUIDE ==========
  Serial.println("🔌 Kết nối điện cực:");
  Serial.println("   RED (RA):    Cổ tay phải");
  Serial.println("   YELLOW (LA): Cổ tay trái");
  Serial.println("   GREEN (RL):  Mắt cá chân hoặc bụng");
  Serial.println();

  // ========== INITIALIZE LCD (if LCD/Dual mode) ==========
#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
  Serial.println("🎨 Initializing LCD...");
  tft.init();
  tft.setRotation(1); // Landscape (180° rotated)
  tft.fillScreen(TFT_BLACK);

  Serial.println("🎨 Initializing ECG UI...");
  ecg_ui_init(&tft);
  Serial.println("✓ LCD ready");
  Serial.println();
#endif

  // ========== INITIALIZE AD8232 ==========
  Serial.println("💓 Initializing AD8232...");
  initAD8232();
  Serial.println("✓ AD8232 ready");
  Serial.println();

  // ========== READY MESSAGE ==========
  Serial.println("✓ System ready!");
  Serial.println("✓ Starting ECG monitoring in 3s...");
  Serial.println();
  delay(3000);

#if defined(ECG_MODE_SERIAL_PLOTTER) || defined(ECG_MODE_DUAL)
  Serial.println("========== ECG DATA STREAM ==========");
#endif
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop()
{
  // -----------------------------------------------
  // 1. UPDATE AD8232 (250Hz internal timing)
  // -----------------------------------------------
  updateAD8232();

  // Get latest ECG data
  float raw = getECGRawSignal();
  float filtered = getECGFilteredSignal();
  int hr = getHeartRate();
  bool leadsOK = areLeadsConnected();

  // -----------------------------------------------
  // 2. LCD UI UPDATE - every iteration (UI downsamples internally)
  // -----------------------------------------------
#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
  ecg_ui_update(filtered, hr, leadsOK);
#endif

  // -----------------------------------------------
  // 3. SERIAL OUTPUT (mode-aware)
  // -----------------------------------------------
#if defined(ECG_MODE_SERIAL_PLOTTER) || defined(ECG_MODE_DUAL)
  static float rawPeak = 0;
  static float filteredPeak = 0;
  static bool windowHasData = false;

  if (!windowHasData || fabsf(raw) > fabsf(rawPeak))
    rawPeak = raw;
  if (!windowHasData || fabsf(filtered) > fabsf(filteredPeak))
    filteredPeak = filtered;
  windowHasData = true;

  if (millis() - lastECGUpdate >= SERIAL_OUTPUT_INTERVAL)
  {
    lastECGUpdate = millis();

    if (!leadsOK)
    {
      Serial.println("Raw:0.00,Filtered:0.00,HR:0");
    }
    else
    {
      Serial.print("Raw:");
      Serial.print(rawPeak, 2);
      Serial.print(",Filtered:");
      Serial.print(filteredPeak, 2);
      Serial.print(",HR:");
      Serial.println(hr);
    }

    windowHasData = false;
    rawPeak = 0;
    filteredPeak = 0;
  }
#endif

  // -----------------------------------------------
  // 4. LVGL HANDLER (LCD/Dual mode)
  // -----------------------------------------------
#if defined(ECG_MODE_LCD_DISPLAY) || defined(ECG_MODE_DUAL)
  ecg_ui_tick();
#endif
}

/*
 * ===================================================================
 * HƯỚNG DẪN SỬ DỤNG
 * ===================================================================
 *
 * 1. SERIAL PLOTTER MODE
 * ----------------------
 * Bật: #define ECG_MODE_SERIAL_PLOTTER
 *
 * Cách xem:
 *   - PlatformIO: Serial Monitor (Terminal)
 *   - Arduino IDE: Tools > Serial Plotter
 *   - TeleplotG Chrome extension (advanced)
 *
 * Output format:
 *   Raw:123.45 Filtered:120.30 HR:75
 *
 * Plotter sẽ tự động vẽ 3 đường:
 *   - Raw signal (đỏ)
 *   - Filtered signal (xanh)
 *   - Heart rate (vàng)
 *
 * Ưu điểm:
 *   + Không cần LCD, chỉ cần USB
 *   + Xem realtime trên PC với màn hình lớn
 *   + Dễ debug, dễ so sánh tín hiệu
 *   + Có thể record/export data
 *
 * Nhược điểm:
 *   - Phải kết nối USB với PC
 *   - Không portable
 *
 *
 * 2. LCD DISPLAY MODE
 * -------------------
 * Bật: #define ECG_MODE_LCD_DISPLAY
 *
 * Features:
 *   - Professional medical-grade UI
 *   - Realtime waveform 250Hz
 *   - Heart rate detection
 *   - Lead-off warning
 *   - Grid background (ECG paper style)
 *
 * Buttons:
 *   UP/DOWN:  Zoom in/out (future feature)
 *   LEFT:     Back (future feature)
 *   RIGHT:    Next screen (future feature)
 *   ENTER:    Menu (future feature)
 *
 * Ưu điểm:
 *   + Standalone device
 *   + Professional appearance
 *   + Portable, không cần PC
 *   + Phù hợp demo, presentation
 *
 * Nhược điểm:
 *   - Cần LCD (tốn chi phí)
 *   - Màn hình nhỏ hơn PC
 *
 *
 * TROUBLESHOOTING
 * ---------------
 * Q: Compile error "No mode selected"?
 * A: Bật 1 trong 2 #define ở đầu file
 *
 * Q: Serial Plotter không hiển thị đồ thị?
 * A: Đảm bảo format đúng "Label:Value"
 *    Kiểm tra baudrate 115200
 *
 * Q: LCD blank screen?
 * A: Kiểm tra TFT_eSPI config trong User_Setup.h
 *    Kiểm tra kết nối SPI (MOSI, SCK, CS, DC, RST)
 *
 * Q: ECG không đúng?
 * A: Kiểm tra electrode placement
 *    Kiểm tra ground connection
 *    Đợi 5-10s để filters ổn định
 *
 * Q: HR detection không chính xác?
 * A: Đảm bảo điện cực dính chặt
 *    Giữ yên, không cử động
 *    Đợi algorithm ổn định (10-15s)
 */
