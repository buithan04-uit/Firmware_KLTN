#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

enum ScreenType
{
    SCR_BOOT,
    SCR_MENU,
    SCR_MONITOR, // Màn hình All-in-one (Giống HTML)
    SCR_ECG,     // Màn hình ECG riêng
    SCR_SPO2,    // Màn hình HR/SpO2 riêng
    SCR_TEMP,    // Màn hình Temperature riêng
    SCR_WIFI_SCAN,
    SCR_WIFI_PASS,
    SCR_CONFIG // Màn hình Web Config (Giống HTML)
};

// Biến toàn cục
extern ScreenType current_screen_type;

void ui_init();
void ui_switch_screen(ScreenType scr);
void ui_update_sensors(float temp, int hr, int spo2, int ecg_val);
void ui_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#endif