/*
 * ===================================================================
 * UI_ECG.H - ECG STANDALONE UI (KHÔNG ẢNH HƯỞNG UI HIỆN TẠI)
 * ===================================================================
 *
 * File UI riêng cho ECG monitoring, tách biệt hoàn toàn với ui.h/ui.cpp
 *
 * FEATURES:
 * - Realtime ECG waveform chart (250Hz capable)
 * - Heart rate display with BPM detection
 * - Lead-off detection warning
 * - Professional medical-grade design
 * - Independent navigation (UP/DOWN/ENTER/BACK)
 *
 * USAGE:
 * 1. #include "ui_ecg.h" trong main.cpp
 * 2. ecg_ui_init() trong setup()
 * 3. ecg_ui_update(ecg_value) trong loop()
 */

#ifndef UI_ECG_H
#define UI_ECG_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

// ==========================================
// PUBLIC API
// ==========================================

/**
 * Khởi tạo ECG UI system
 * Gọi 1 lần trong setup()
 *
 * @param tft_instance: Pointer tới TFT_eSPI object
 */
void ecg_ui_init(TFT_eSPI *tft_instance);

/**
 * Update ECG waveform realtime
 * Gọi trong loop() với tần số 250Hz (mỗi 4ms)
 *
 * @param ecg_mV: ECG signal value in millivolts (-3.0 to +3.0)
 * @param hr_bpm: Heart rate in beats per minute (0-220)
 * @param leads_connected: true nếu điện cực được kết nối
 */
void ecg_ui_update(float ecg_mV, int hr_bpm, bool leads_connected);

/**
 * LVGL task handler - gọi trong loop()
 * Cần gọi ít nhất mỗi 5-10ms để UI mượt
 */
void ecg_ui_tick();

/**
 * Đọc input từ buttons
 * Tự động được gọi bởi LVGL, không cần gọi thủ công
 */
void ecg_button_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

/**
 * Reset chart về trạng thái ban đầu
 */
void ecg_ui_reset_chart();

/**
 * Hiển thị/ẩn grid background
 *
 * @param show: true để hiện grid, false để ẩn
 */
void ecg_ui_show_grid(bool show);

/**
 * Thay đổi màu sắc waveform
 *
 * @param color_hex: Hex color code (ví dụ: 0x00FF00 = green)
 */
void ecg_ui_set_waveform_color(uint32_t color_hex);

#endif // UI_ECG_H
