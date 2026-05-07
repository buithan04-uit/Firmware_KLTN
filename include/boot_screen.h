#pragma once

#include <Arduino.h>
#include <lvgl.h>

// Trạng thái hiển thị cho từng sensor trên boot screen
enum BootSensorStatus
{
    BOOT_SENSOR_CHECKING = 0, // "..."  - màu xám  (đang kiểm tra)
    BOOT_SENSOR_OK,           // "OK"   - màu xanh lá
    BOOT_SENSOR_RETRY,        // "RETRY"- màu vàng (không thấy, thử lại)
    BOOT_SENSOR_FAIL,         // "FAIL" - màu đỏ   (lỗi hẳn)
    BOOT_SENSOR_OFF,          // "--"   - màu xám đậm (không dùng)
};

// Chỉ số sensor (dùng với boot_screen_set_sensor_status)
#define BOOT_SENSOR_AD8232 0
#define BOOT_SENSOR_MAX30102 1
#define BOOT_SENSOR_MLX90614 2
#define BOOT_SENSOR_WIFI 3
#define BOOT_SENSOR_VL53L0X 4
#define BOOT_SENSOR_COUNT 5

// Xây dựng màn hình boot (gọi từ ui_switch_screen SCR_BOOT)
void build_boot(void);

// Cập nhật trạng thái sensor (gọi sau mỗi bước init)
void boot_screen_set_sensor_status(uint8_t sensor_index, BootSensorStatus status);

// Cập nhật thanh progress 0-100
void boot_screen_set_progress(uint8_t percent);

// Cập nhật dòng trạng thái (vi du: "Initializing MLX90614...")
void boot_screen_set_status_text(const char *text);

// Hien thi SSID hoac IP WiFi tren card Network
void boot_screen_set_wifi_detail(const char *ssid, const char *ip);

// Refresh gia tri Heap hien thi
void boot_screen_refresh_heap(void);