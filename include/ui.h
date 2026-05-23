#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

enum ScreenType
{
    SCR_BOOT,
    SCR_MENU,
    SCR_MONITOR,     // Màn hình All-in-one (Giống HTML)
    SCR_ECG,         // Màn hình ECG riêng
    SCR_SPO2,        // Màn hình HR/SpO2 riêng
    SCR_TEMP,        // Màn hình Temperature riêng
    SCR_COLLECTDATA, // Màn hình thu thập dữ liệu
    SCR_MEASUREALL,  // Màn hình đo tất cả & gửi MQTT
    SCR_WIFI_SCAN,
    SCR_WIFI_PASS,
    SCR_CONFIG // Màn hình Web Config (Giống HTML)
};

// Biến toàn cục
extern ScreenType current_screen_type;

void ui_init();
void ui_switch_screen(ScreenType scr);
void ui_update_sensors(float temp, int hr, int spo2, int ecg_val);
void ui_update_ecg_live(float ecg_mv, int hr_bpm, bool leads_connected);
void ui_set_ambient_temp(float temp);
void ui_set_temp_distance(float distMm);
void ui_set_sensor_status(const char *msg, uint32_t colorHex);
void ui_set_config_ip(const char *ip);
void ui_set_config_status(const char *msg, uint32_t colorHex);
void ui_set_config_instruction(const char *msg);
bool ui_consume_config_apply_request();
bool ui_consume_wifi_connect_request(String &ssid, String &pass);
bool ui_consume_mqtt_send_toggle_request();
bool ui_consume_collect_take_request();
bool ui_consume_collect_id_minus_request();
bool ui_consume_collect_id_plus_request();
bool ui_consume_collect_reset_request();
bool ui_consume_measure_all_start_request();
void ui_set_wifi_connect_feedback(const char *msg, uint32_t colorHex);
void ui_set_header_wifi(const char *signalLevel, const char *ssid, uint32_t colorHex);
void ui_set_mqtt_status(const char *msg, uint32_t colorHex);
void ui_set_measure_all_values(float temp, int hr, int spo2, float ecg, float dist);
void ui_set_measure_all_status(const char *msg, uint32_t colorHex);
void ui_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
void ui_update_measure_all_ecg_waveform(int waveformY200, bool leadsOn);
void ui_set_measure_all_send_state(uint8_t state);
void ui_update_measureall_ecg(float ecg_mv, bool leads_connected);
void ui_set_battery(uint8_t percent, bool charging, bool full = false);
// 0=IDLE  1=SENDING  2=OK  3=FAIL  4=NO_WIFI

#endif