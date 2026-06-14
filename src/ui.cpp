#include "ui.h"
#include "custom_icons.h"
#include "ui_datacollector.h"
#include <WiFi.h>
#include "boot_screen.h"
// --- OPTIONAL: CUSTOM FONT ICONS (Font Awesome) ---
// Uncomment sau khi generate font file từ https://lvgl.io/tools/fontconverter
// Xem hướng dẫn chi tiết trong FONT_ICON_GUIDE.md
// LV_FONT_DECLARE(lv_font_fa_24);

// --- 1. CẤU HÌNH PHẦN CỨNG ---
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_LEFT 25 // Nút Back/Trái
#define BTN_RIGHT 26
#define BTN_ENTER 27

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// --- 2. BIẾN TOÀN CỤC ---
extern TFT_eSPI tft; // Defined in main.cpp
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 20]; // Buffer nhỏ để tiết kiệm RAM

lv_group_t *input_group;
lv_obj_t *current_screen_obj = NULL;
ScreenType current_screen_type = SCR_BOOT;

// Các Widget hiển thị dữ liệu (để update từ main)
lv_obj_t *chart_ecg = NULL;
lv_chart_series_t *ser_ecg = NULL;
lv_obj_t *lbl_hr_val = NULL;
lv_obj_t *lbl_spo2_val = NULL;
lv_obj_t *lbl_temp_val = NULL;
lv_obj_t *lbl_temp_env_val = NULL;
lv_obj_t *lbl_temp_dist_val = NULL;
lv_obj_t *lbl_sensor_status = NULL;
lv_obj_t *lbl_config_ip = NULL;
lv_obj_t *lbl_config_status = NULL;
lv_obj_t *lbl_config_instruction = NULL;
bool g_config_apply_requested = false;
bool g_mqtt_send_toggle_requested = false;
static String g_scanned_ssids[20];
static int g_scanned_count = 0;
static String g_selected_wifi_ssid = "";
static bool g_wifi_connect_requested = false;
static String g_wifi_connect_ssid = "";
static String g_wifi_connect_pass = "";
static lv_obj_t *lbl_wifi_pass_status = NULL;
static lv_obj_t *lbl_wifi_selected_ssid = NULL;
static lv_obj_t *ta_wifi_pass = NULL;
static lv_obj_t *wifi_scan_list = NULL;
static lv_obj_t *wifi_scan_loading_lbl = NULL;
static lv_obj_t *wifi_scan_spinner = NULL;
static lv_timer_t *wifi_scan_timer = NULL;
static uint32_t wifi_scan_anim_tick = 0;
static uint8_t wifi_scan_retry_count = 0;
static uint32_t wifi_scan_started_ms = 0;
static lv_obj_t *lbl_header_wifi_icon = NULL;
static lv_obj_t *lbl_header_wifi_signal = NULL;
static lv_obj_t *lbl_header_wifi_ssid = NULL;
static lv_obj_t *lbl_header_mqtt_status = NULL;
static lv_obj_t *lbl_battery_percent = NULL;
static lv_obj_t *lbl_battery_icon = NULL;
static lv_obj_t *lbl_battery_charge_icon = NULL;
static lv_obj_t *bar_battery_level = NULL;
static lv_obj_t *ecg_lbl_warning = NULL;
static lv_obj_t *ecg_beat_dot = NULL;

static lv_obj_t *lbl_all_hr = NULL;
static lv_obj_t *lbl_all_spo2 = NULL;
static lv_obj_t *lbl_all_temp = NULL;
static lv_obj_t *lbl_all_ecg = NULL;
static lv_obj_t *lbl_all_dist = NULL;
static lv_obj_t *lbl_all_status = NULL;
static lv_obj_t *lbl_all_ecg_state = NULL;
static lv_obj_t *lbl_all_ecg_quality = NULL;
static lv_obj_t *lbl_all_ecg_amp = NULL;
static lv_obj_t *all_ecg_beat_dot = NULL;
static lv_obj_t *chart_ecg_mini = NULL;
static lv_chart_series_t *ser_ecg_mini = NULL;

static bool g_collect_take_requested = false;
static bool g_collect_id_minus_requested = false;
static bool g_collect_id_plus_requested = false;
static bool g_collect_reset_requested = false;
static bool g_measure_all_start_requested = false;
static bool g_collect_session_plus_requested = false;

static bool is_measurement_screen_ui(ScreenType scr)
{
    return (scr == SCR_MONITOR || scr == SCR_ECG || scr == SCR_SPO2 || scr == SCR_TEMP || scr == SCR_MEASUREALL);
}

static void lock_scroll(lv_obj_t *obj)
{
    if (!obj)
    {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

// --- 3. KHAI BÁO PROTOTYPE (Tránh lỗi "Not Declared") ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void ui_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
void init_styles();
void build_menu();
void build_monitor();
void build_ecg();
void build_spo2();
void build_temp();
void build_collectdata();
void build_measureall();
void build_config();
void build_wifi_scan();
void build_wifi_pass();
void draw_lungs_icon(lv_obj_t *parent, int16_t x, int16_t y, lv_color_t color);
void draw_thermometer_icon(lv_obj_t *parent, int16_t x, int16_t y, lv_color_t color);
void wifi_scan_tick(lv_timer_t *timer);
void wifi_scan_render_results();
void start_wifi_scan_async();
void build_boot();

// Dialog state
static lv_obj_t *active_dialog = NULL;
static lv_obj_t *dialog_btn_yes = NULL;
static lv_obj_t *dialog_btn_no = NULL;
static uint8_t dialog_focused_btn = 0;       // 0=YES, 1=NO
static uint32_t last_screen_change_time = 0; // Track screen changes for input debounce
static uint32_t last_dialog_close_time = 0;  // Track dialog close to prevent immediate re-trigger

// --- 4. STYLES (GIAO DIỆN DARK) ---
static lv_style_t style_screen;
static lv_style_t style_panel;
static lv_style_t style_focus;
static lv_style_t style_text_big;

void init_styles()
{
    // Nền Đen tối
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x000000));
    lv_style_set_text_color(&style_screen, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&style_screen, &lv_font_montserrat_14);

    // Panel Xám Tối với viền sáng hơn
    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0x1a1a1a));
    lv_style_set_radius(&style_panel, 8);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_border_color(&style_panel, lv_color_hex(0x404040));
    lv_style_set_shadow_width(&style_panel, 8);
    lv_style_set_shadow_color(&style_panel, lv_color_hex(0x000000));
    lv_style_set_shadow_ofs_y(&style_panel, 4);
    lv_style_set_shadow_spread(&style_panel, 2);

    // Hiệu ứng Chọn (Viền Cyan sáng + glow)
    lv_style_init(&style_focus);
    lv_style_set_border_width(&style_focus, 3);
    lv_style_set_border_color(&style_focus, lv_color_hex(0x00E5FF));
    lv_style_set_bg_color(&style_focus, lv_color_hex(0x0a2a2a));
    lv_style_set_shadow_width(&style_focus, 20);
    lv_style_set_shadow_color(&style_focus, lv_color_hex(0x00E5FF));
    lv_style_set_shadow_ofs_y(&style_focus, 0);
    lv_style_set_shadow_spread(&style_focus, 5);

    // Font Số To
    lv_style_init(&style_text_big);
    lv_style_set_text_font(&style_text_big, &lv_font_montserrat_28);
}

// --- 5. DRIVER HIỂN THỊ (FLUSH) ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// --- 6. DRIVER NHẬP LIỆU (NÚT BẤM) ---
void ui_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static uint32_t last_debug = 0;

    // Long press repeat for navigation
    static uint32_t up_press_start = 0;
    static uint32_t down_press_start = 0;
    static uint32_t left_press_start = 0;
    static uint32_t right_press_start = 0;
    static uint32_t last_repeat = 0;
    const uint32_t REPEAT_DELAY = 500;     // Wait 500ms before repeat
    const uint32_t REPEAT_RATE = 150;      // Repeat every 150ms
    const uint32_t EXIT_EDIT_DELAY = 1000; // Hold LEFT 1s to exit editing

    // Track if LEFT already exited editing in this hold
    static bool left_exited_editing = false;
    static bool left_back_sent = false;

    // Button debouncing - track previous states
    static bool prev_enter = HIGH;
    static bool prev_up = HIGH;
    static bool prev_down = HIGH;
    static bool prev_right = HIGH;
    static bool prev_left = HIGH;

    // Read current button states
    bool curr_enter = digitalRead(BTN_ENTER);
    bool curr_up = digitalRead(BTN_UP);
    bool curr_down = digitalRead(BTN_DOWN);
    bool curr_right = digitalRead(BTN_RIGHT);
    bool curr_left = digitalRead(BTN_LEFT);

    data->state = LV_INDEV_STATE_REL;

    // Skip input for 300ms after screen change to prevent false triggers
    if (millis() - last_screen_change_time < 300)
    {
        // Update prev states even when skipping
        prev_enter = curr_enter;
        prev_up = curr_up;
        prev_down = curr_down;
        prev_right = curr_right;
        prev_left = curr_left;
        return;
    }

    // Nếu dialog đang mở, xử lý riêng (edge detection)
    if (active_dialog != NULL)
    {
        if (curr_left == LOW && prev_left == HIGH) // LEFT button pressed
        {
            Serial.println("[DIALOG] LEFT - Focus YES");
            dialog_focused_btn = 0;
            // Change colors directly for clear visual feedback
            lv_obj_set_style_bg_color(dialog_btn_yes, lv_color_hex(0x00ffaa), 0);
            lv_obj_set_style_border_width(dialog_btn_yes, 4, 0);
            lv_obj_set_style_border_color(dialog_btn_yes, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_color(dialog_btn_no, lv_color_hex(0xFF1744), 0);
            lv_obj_set_style_border_width(dialog_btn_no, 0, 0);
            data->state = LV_INDEV_STATE_PR;
        }
        else if (curr_right == LOW && prev_right == HIGH) // RIGHT button pressed
        {
            Serial.println("[DIALOG] RIGHT - Focus NO");
            dialog_focused_btn = 1;
            // Change colors directly for clear visual feedback
            lv_obj_set_style_bg_color(dialog_btn_yes, lv_color_hex(0x00E676), 0);
            lv_obj_set_style_border_width(dialog_btn_yes, 0, 0);
            lv_obj_set_style_bg_color(dialog_btn_no, lv_color_hex(0xff3366), 0);
            lv_obj_set_style_border_width(dialog_btn_no, 4, 0);
            lv_obj_set_style_border_color(dialog_btn_no, lv_color_hex(0xFFFFFF), 0);
            data->state = LV_INDEV_STATE_PR;
        }
        else if (curr_enter == LOW && prev_enter == HIGH) // ENTER button pressed
        {
            Serial.printf("[DIALOG] ENTER - Button %d clicked\n", dialog_focused_btn);
            if (dialog_focused_btn == 0)
            {
                // YES clicked
                lv_obj_del(active_dialog);
                active_dialog = NULL;
                dialog_btn_yes = NULL;
                dialog_btn_no = NULL;
                dialog_focused_btn = 0;
                ui_switch_screen(SCR_MENU);
            }
            else
            {
                // NO clicked - just close dialog, stay on current screen
                Serial.println("[DIALOG] NO - Staying on screen");
                lv_obj_del(active_dialog);
                active_dialog = NULL;
                dialog_btn_yes = NULL;
                dialog_btn_no = NULL;
                dialog_focused_btn = 0;
                // Reset timers to prevent immediate re-trigger
                last_screen_change_time = millis();
                last_dialog_close_time = millis();
            }
            data->state = LV_INDEV_STATE_PR;
        }

        // Update previous states and return
        prev_enter = curr_enter;
        prev_up = curr_up;
        prev_down = curr_down;
        prev_right = curr_right;
        prev_left = curr_left;
        return; // Ignore other keys when dialog open
    }

    // Đọc các nút theo thứ tự ưu tiên (EDGE DETECTION - chỉ trigger khi nhấn, không phải giữ)
    if (curr_enter == LOW && prev_enter == HIGH) // ENTER pressed
    {
        if (is_measurement_screen_ui(current_screen_type))
        {
            // In measurement screens, reserve ENTER for manual MQTT send ON/OFF toggle.
            g_mqtt_send_toggle_requested = true;
            Serial.println("[MQTT][CTRL] ENTER -> toggle send request");
            data->state = LV_INDEV_STATE_REL;
        }
        else
        {
            Serial.println("BTN: ENTER");
            data->key = LV_KEY_ENTER;
            data->state = LV_INDEV_STATE_PR;
        }
    }
    else if (curr_up == LOW) // UP pressed or held
    {
        if (prev_up == HIGH) // Just pressed
        {
            Serial.println("BTN: UP");
            up_press_start = millis();
            last_repeat = millis();

            // Debug current state
            Serial.printf("[UP_DEBUG] Screen=%d, Editing=%d\n", current_screen_type, lv_group_get_editing(input_group));

            // Ở Menu: dùng UP cho 2D navigation, screens khác: dùng PREV
            if (current_screen_type == SCR_MENU)
            {
                Serial.println("[UP_DEBUG] → Using LV_KEY_UP for menu");
                data->key = LV_KEY_UP;
            }
            else if (current_screen_type == SCR_WIFI_PASS)
            {
                // If editing: send UP to widget (keyboard/textarea)
                // If not editing: send PREV for navigation
                if (lv_group_get_editing(input_group))
                {
                    Serial.println("[WIFI_PASS] UP - Control widget");
                    data->key = LV_KEY_UP;
                }
                else
                {
                    Serial.println("[WIFI_PASS] UP - Navigate");
                    data->key = LV_KEY_PREV;
                }
            }
            else if (current_screen_type == SCR_COLLECTDATA)
            {
                data->key = LV_KEY_UP;
            }
            else
            {
                Serial.printf("[UP_DEBUG] → Other screen (%d), using LV_KEY_PREV\n", current_screen_type);
                data->key = LV_KEY_PREV;
            }
            data->state = LV_INDEV_STATE_PR;
        }
        else // Holding - repeat after delay
        {
            uint32_t held = millis() - up_press_start;
            uint32_t since_repeat = millis() - last_repeat;

            if (held >= REPEAT_DELAY && since_repeat >= REPEAT_RATE)
            {
                last_repeat = millis();

                if (current_screen_type == SCR_MENU)
                {
                    data->key = LV_KEY_UP;
                }
                else if (current_screen_type == SCR_WIFI_PASS)
                {
                    // Repeat based on editing state
                    if (lv_group_get_editing(input_group))
                    {
                        data->key = LV_KEY_UP; // Control widget
                    }
                    else
                    {
                        data->key = LV_KEY_PREV; // Navigate
                    }
                }
                else if (current_screen_type == SCR_COLLECTDATA)
                {
                    data->key = LV_KEY_UP;
                }
                else
                {
                    data->key = LV_KEY_PREV;
                }
                data->state = LV_INDEV_STATE_PR;
            }
        }
    }
    else if (prev_up == LOW) // Released
    {
        up_press_start = 0;
    }
    else if (curr_down == LOW) // DOWN pressed or held
    {
        if (prev_down == HIGH) // Just pressed
        {
            Serial.println("BTN: DOWN");
            down_press_start = millis();
            last_repeat = millis();

            // Ở Menu: dùng DOWN cho 2D navigation, screens khác: dùng NEXT
            if (current_screen_type == SCR_MENU)
            {
                data->key = LV_KEY_DOWN;
            }
            else if (current_screen_type == SCR_WIFI_PASS)
            {
                // If editing: send DOWN to widget (keyboard/textarea)
                // If not editing: send NEXT for navigation
                if (lv_group_get_editing(input_group))
                {
                    Serial.println("[WIFI_PASS] DOWN - Control widget");
                    data->key = LV_KEY_DOWN;
                }
                else
                {
                    Serial.println("[WIFI_PASS] DOWN - Navigate");
                    data->key = LV_KEY_NEXT;
                }
            }
            else if (current_screen_type == SCR_COLLECTDATA)
            {
                data->key = LV_KEY_DOWN;
            }
            else
            {
                data->key = LV_KEY_NEXT;
            }
            data->state = LV_INDEV_STATE_PR;
        }
        else // Holding - repeat after delay
        {
            uint32_t held = millis() - down_press_start;
            uint32_t since_repeat = millis() - last_repeat;

            if (held >= REPEAT_DELAY && since_repeat >= REPEAT_RATE)
            {
                last_repeat = millis();

                if (current_screen_type == SCR_MENU)
                {
                    data->key = LV_KEY_DOWN;
                }
                else if (current_screen_type == SCR_WIFI_PASS)
                {
                    // Repeat based on editing state
                    if (lv_group_get_editing(input_group))
                    {
                        data->key = LV_KEY_DOWN; // Control widget
                    }
                    else
                    {
                        data->key = LV_KEY_NEXT; // Navigate
                    }
                }
                else if (current_screen_type == SCR_COLLECTDATA)
                {
                    data->key = LV_KEY_DOWN;
                }
                else
                {
                    data->key = LV_KEY_NEXT;
                }
                data->state = LV_INDEV_STATE_PR;
            }
        }
    }
    else if (prev_down == LOW) // Released
    {
        down_press_start = 0;
    }
    else if (curr_right == LOW) // RIGHT pressed or held
    {
        if (prev_right == HIGH) // Just pressed
        {
            Serial.println("BTN: RIGHT");
            right_press_start = millis();
            last_repeat = millis();

            // Ở Menu: dùng RIGHT cho 2D navigation, password screen: keyboard navigation, screens khác: dùng NEXT
            if (current_screen_type == SCR_MENU)
            {
                data->key = LV_KEY_RIGHT;
            }
            else if (current_screen_type == SCR_WIFI_PASS)
            {
                data->key = LV_KEY_RIGHT; // For keyboard horizontal navigation
            }
            else if (current_screen_type == SCR_COLLECTDATA)
            {
                data->key = LV_KEY_RIGHT;
            }
            else
            {
                data->key = LV_KEY_NEXT;
            }
            data->state = LV_INDEV_STATE_PR;
        }
        else // Holding - repeat after delay
        {
            uint32_t held = millis() - right_press_start;
            uint32_t since_repeat = millis() - last_repeat;

            if (held >= REPEAT_DELAY && since_repeat >= REPEAT_RATE)
            {
                last_repeat = millis();

                if (current_screen_type == SCR_MENU)
                {
                    data->key = LV_KEY_RIGHT;
                }
                else if (current_screen_type == SCR_WIFI_PASS)
                {
                    data->key = LV_KEY_RIGHT;
                }
                else if (current_screen_type == SCR_COLLECTDATA)
                {
                    data->key = LV_KEY_RIGHT;
                }
                else
                {
                    data->key = LV_KEY_NEXT;
                }
                data->state = LV_INDEV_STATE_PR;
            }
        }
    }
    else if (prev_right == LOW) // Released
    {
        right_press_start = 0;
    }

    // LEFT button - special handling for different screens
    if (current_screen_type == SCR_MENU)
    {
        // Menu: LEFT navigation with long press repeat
        if (curr_left == LOW)
        {
            if (prev_left == HIGH) // Just pressed
            {
                Serial.println("BTN: LEFT (Menu navigation)");
                left_press_start = millis();
                last_repeat = millis();
                data->key = LV_KEY_LEFT;
                data->state = LV_INDEV_STATE_PR;
            }
            else // Holding - repeat
            {
                uint32_t held = millis() - left_press_start;
                uint32_t since_repeat = millis() - last_repeat;

                if (held >= REPEAT_DELAY && since_repeat >= REPEAT_RATE)
                {
                    last_repeat = millis();
                    data->key = LV_KEY_LEFT;
                    data->state = LV_INDEV_STATE_PR;
                }
            }
        }
        else if (prev_left == LOW) // Released
        {
            left_press_start = 0;
        }
    }
    else if (current_screen_type == SCR_WIFI_PASS)
    {
        // Password screen: LEFT for keyboard navigation when editing, ESC when not
        if (curr_left == LOW)
        {
            if (prev_left == HIGH) // Just pressed
            {
                if (lv_group_get_editing(input_group))
                {
                    // In editing mode - LEFT navigates keyboard
                    Serial.println("BTN: LEFT (Keyboard nav)");
                    left_press_start = millis();
                    last_repeat = millis();
                    left_exited_editing = false; // Reset flag
                    data->key = LV_KEY_LEFT;
                    data->state = LV_INDEV_STATE_PR;
                }
                else
                {
                    // Not editing - send ESC to go back
                    Serial.println("BTN: LEFT (Go back)");
                    data->key = LV_KEY_ESC;
                    data->state = LV_INDEV_STATE_PR;
                }
            }
            else if (lv_group_get_editing(input_group)) // Holding in edit mode
            {
                uint32_t held = millis() - left_press_start;
                uint32_t since_repeat = millis() - last_repeat;

                // Hold for 1 second to exit editing mode
                if (held >= EXIT_EDIT_DELAY && !left_exited_editing)
                {
                    Serial.println("BTN: LEFT (HOLD 1s - Exit editing)");
                    lv_group_set_editing(input_group, false);
                    left_exited_editing = true;
                }
                // Normal repeat (before 1 second threshold)
                else if (held < EXIT_EDIT_DELAY && held >= REPEAT_DELAY && since_repeat >= REPEAT_RATE)
                {
                    last_repeat = millis();
                    data->key = LV_KEY_LEFT;
                    data->state = LV_INDEV_STATE_PR;
                }
            }
        }
        else if (prev_left == LOW) // Released
        {
            left_press_start = 0;
            left_exited_editing = false; // Reset flag
        }
    }
    else if (current_screen_type == SCR_COLLECTDATA)
    {
        if (curr_left == LOW)
        {
            if (prev_left == HIGH)
            {
                Serial.println("BTN: LEFT (CollectData - ID minus)");
                left_press_start = millis();
                left_back_sent = false;
                data->key = LV_KEY_LEFT;
                data->state = LV_INDEV_STATE_PR;
            }
            else
            {
                uint32_t held = millis() - left_press_start;
                if (held >= EXIT_EDIT_DELAY && !left_back_sent)
                {
                    Serial.println("BTN: LEFT (CollectData - HOLD BACK)");
                    left_back_sent = true;
                    data->key = LV_KEY_ESC;
                    data->state = LV_INDEV_STATE_PR;
                }
            }
        }
        else if (prev_left == LOW)
        {
            left_press_start = 0;
            left_back_sent = false;
        }
    }
    else
    {
        // All other screens (WIFI_SCAN, MONITOR, ECG, SPO2, TEMP, CONFIG):
        // Immediate back with single press (edge detection)
        if (curr_left == LOW && prev_left == HIGH)
        {
            Serial.println("[LEFT] Immediate back - ESC");
            data->key = LV_KEY_ESC;
            data->state = LV_INDEV_STATE_PR;
        }
    }

    // Update previous button states for next call
    prev_enter = curr_enter;
    prev_up = curr_up;
    prev_down = curr_down;
    prev_right = curr_right;
    prev_left = curr_left;

    // Debug group info mỗi 2 giây
    if (data->state == LV_INDEV_STATE_PR && millis() - last_debug > 2000)
    {
        last_debug = millis();
        if (input_group)
        {
            uint32_t obj_count = lv_group_get_obj_count(input_group);
            lv_obj_t *focused = lv_group_get_focused(input_group);
            bool editing = lv_group_get_editing(input_group);
            bool wrap = lv_group_get_wrap(input_group);
            Serial.printf("[DEBUG] Group: %d objs, Focused: %p, Screen: %d, Editing: %d, Wrap: %d\n",
                          obj_count, focused, current_screen_type, editing, wrap);
        }
        else
        {
            Serial.println("[DEBUG] ERROR: input_group is NULL!");
        }
    }
}

// --- 7. HELPER FUNCTIONS ---

// Confirmation Dialog Callback
void confirm_back_yes(lv_event_t *e)
{
    Serial.println("[CONFIRM] YES -> Going back to MENU");
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(mbox);
    ui_switch_screen(SCR_MENU);
}

void confirm_back_no(lv_event_t *e)
{
    Serial.println("[CONFIRM] NO -> Staying on current screen");
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(mbox);
}

// Custom Confirm Dialog with 2 Separate Buttons
void show_confirm_dialog()
{
    // Prevent creating multiple dialogs
    if (active_dialog != NULL)
    {
        Serial.println("[DIALOG] Already exists, skipping");
        return;
    }

    // Create dark overlay
    active_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(active_dialog, 320, 240);
    lv_obj_set_style_bg_color(active_dialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(active_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(active_dialog, 0, 0);
    lv_obj_clear_flag(active_dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Dialog box
    lv_obj_t *box = lv_obj_create(active_dialog);
    lv_obj_set_size(box, 260, 140);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_grad_color(box, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(box, 3, 0);
    lv_obj_set_style_shadow_width(box, 30, 0);
    lv_obj_set_style_shadow_color(box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(box, 12, 0);

    // Title
    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, LV_SYMBOL_WARNING " CONFIRM");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFD600), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    // Message
    lv_obj_t *msg = lv_label_create(box);
    lv_label_set_text(msg, "Return to Main Menu?");
    lv_obj_set_style_text_color(msg, lv_color_hex(0xDDDDDD), 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);

    // YES Button (Left) - Focused by default
    dialog_btn_yes = lv_btn_create(box);
    lv_obj_set_size(dialog_btn_yes, 80, 35);
    lv_obj_align(dialog_btn_yes, LV_ALIGN_BOTTOM_MID, -55, -15);
    lv_obj_set_style_bg_color(dialog_btn_yes, lv_color_hex(0x00ffaa), 0);
    lv_obj_set_style_border_width(dialog_btn_yes, 4, 0);
    lv_obj_set_style_border_color(dialog_btn_yes, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(dialog_btn_yes, 8, 0);

    lv_obj_t *yes_lbl = lv_label_create(dialog_btn_yes);
    lv_label_set_text(yes_lbl, "YES");
    lv_obj_set_style_text_color(yes_lbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(yes_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(yes_lbl);

    // NO Button (Right)
    dialog_btn_no = lv_btn_create(box);
    lv_obj_set_size(dialog_btn_no, 80, 35);
    lv_obj_align(dialog_btn_no, LV_ALIGN_BOTTOM_MID, 55, -15);
    lv_obj_set_style_bg_color(dialog_btn_no, lv_color_hex(0xFF1744), 0);
    lv_obj_set_style_radius(dialog_btn_no, 8, 0);

    lv_obj_t *no_lbl = lv_label_create(dialog_btn_no);
    lv_label_set_text(no_lbl, "NO");
    lv_obj_set_style_text_color(no_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(no_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(no_lbl);

    // Focus YES by default
    dialog_focused_btn = 0;

    Serial.println("[DIALOG] Created - Use LEFT/RIGHT to select, ENTER to confirm");
}

void handle_back_key(lv_event_t *e)
{
    // Debounce - only process once per 500ms
    static uint32_t last_esc_time = 0;

    // Don't process if dialog is already open
    if (active_dialog != NULL)
    {
        Serial.println("[handle_back_key] Dialog already open, ignoring");
        return;
    }

    // Don't process if dialog was just closed (within 500ms)
    uint32_t now = millis();
    if (now - last_dialog_close_time < 500)
    {
        Serial.println("[handle_back_key] Dialog just closed, ignoring");
        return;
    }

    uint32_t key = lv_indev_get_key(lv_indev_get_act());
    lv_event_code_t code = lv_event_get_code(e);

    // Only process KEY event with ESC key
    if (code != LV_EVENT_KEY || key != LV_KEY_ESC)
    {
        return;
    }

    // Debounce check
    if (now - last_esc_time < 500)
    {
        Serial.println("[handle_back_key] Debounced - too soon");
        return;
    }
    last_esc_time = now;

    Serial.printf("[handle_back_key] ESC processed, Screen: %d\n", current_screen_type);

    if (current_screen_type == SCR_WIFI_PASS)
    {
        Serial.println("[BACK] WIFI_PASS -> WIFI_SCAN");
        ui_switch_screen(SCR_WIFI_SCAN);
    }
    else if (current_screen_type != SCR_MENU)
    {
        // Show confirmation dialog
        show_confirm_dialog();
    }
}

void clean_resources()
{
    if (wifi_scan_timer)
    {
        lv_timer_del(wifi_scan_timer);
        wifi_scan_timer = NULL;
    }

    if (input_group)
        lv_group_remove_all_objs(input_group);
    lbl_hr_val = NULL;
    lbl_spo2_val = NULL;
    lbl_temp_val = NULL;
    lbl_temp_env_val = NULL;
    lbl_temp_dist_val = NULL;
    lbl_sensor_status = NULL;
    lbl_config_ip = NULL;
    lbl_config_status = NULL;
    lbl_config_instruction = NULL;
    lbl_wifi_pass_status = NULL;
    lbl_wifi_selected_ssid = NULL;
    ta_wifi_pass = NULL;
    wifi_scan_list = NULL;
    wifi_scan_loading_lbl = NULL;
    wifi_scan_spinner = NULL;
    wifi_scan_anim_tick = 0;
    wifi_scan_retry_count = 0;
    wifi_scan_started_ms = 0;
    lbl_header_wifi_icon = NULL;
    lbl_header_wifi_signal = NULL;
    lbl_header_wifi_ssid = NULL;
    lbl_header_mqtt_status = NULL;
    ecg_lbl_warning = NULL;
    ecg_beat_dot = NULL;
    chart_ecg = NULL;
    ser_ecg = NULL;
    chart_ecg_mini = NULL;
    ser_ecg_mini = NULL;
    lbl_all_hr = NULL;
    lbl_all_spo2 = NULL;
    lbl_all_temp = NULL;
    lbl_all_ecg = NULL;
    lbl_all_dist = NULL;
    lbl_all_status = NULL;
    lbl_all_ecg_state = NULL;
    lbl_all_ecg_quality = NULL;
    lbl_all_ecg_amp = NULL;
    all_ecg_beat_dot = NULL;
}

void switch_to_obj(lv_obj_t *obj, ScreenType type)
{
    lv_scr_load(obj);
    // Xóa màn cũ để tiết kiệm RAM
    if (current_screen_obj && current_screen_obj != obj)
    {
        lv_obj_del(current_screen_obj);
    }
    current_screen_obj = obj;
    current_screen_type = type;

    // Reset dialog state khi chuyển màn hình
    active_dialog = NULL;
    dialog_btn_yes = NULL;
    dialog_btn_no = NULL;
    dialog_focused_btn = 0;

    // Record screen change time for input debounce
    last_screen_change_time = millis();
}

void create_header(lv_obj_t *parent, const char *title)
{
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_set_size(h, 320, 30);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 0);
    lock_scroll(h);
    lv_obj_set_style_bg_color(h, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(h, 1, 0);
    lv_obj_set_style_border_color(h, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *l = lv_label_create(h);
    lv_label_set_text(l, title);
    lv_obj_set_width(l, 75);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(l, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 5, 0);

    lbl_header_mqtt_status = lv_label_create(h);
    lv_label_set_text(lbl_header_mqtt_status, "SEND OFF");
    lv_obj_set_width(lbl_header_mqtt_status, 45);
    lv_label_set_long_mode(lbl_header_mqtt_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_header_mqtt_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_header_mqtt_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_header_mqtt_status, lv_color_hex(0xFFB300), 0);
    lv_obj_align(lbl_header_mqtt_status, LV_ALIGN_LEFT_MID, 80, 0);

    // ========== NHÓM BATTERY ==========
    lbl_battery_icon = lv_label_create(h);
    lv_label_set_text(lbl_battery_icon, ICON_BATTERY);
    lv_obj_set_style_text_font(lbl_battery_icon, &lv_font_battery_24, 0);
    lv_obj_set_style_text_color(lbl_battery_icon, lv_color_hex(0x00E676), 0);
    lv_obj_align(lbl_battery_icon, LV_ALIGN_LEFT_MID, 130, 0);

    lbl_battery_charge_icon = lv_label_create(h);
    lv_label_set_text(lbl_battery_charge_icon, ICON_CHARGE);
    lv_obj_set_style_text_font(lbl_battery_charge_icon, &lv_font_charge_24, 0);
    lv_obj_set_style_text_color(lbl_battery_charge_icon, lv_color_hex(0x29B6F6), 0);
    lv_obj_align(lbl_battery_charge_icon, LV_ALIGN_LEFT_MID, 136, -2);
    lv_obj_add_flag(lbl_battery_charge_icon, LV_OBJ_FLAG_HIDDEN);

    lbl_battery_percent = lv_label_create(h);
    lv_label_set_text(lbl_battery_percent, "--");
    lv_obj_set_width(lbl_battery_percent, 30);
    lv_label_set_long_mode(lbl_battery_percent, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_battery_percent, LV_TEXT_ALIGN_LEFT, 0); // Đổi thành LEFT
    lv_obj_set_style_text_font(lbl_battery_percent, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_battery_percent, lv_color_hex(0x00E676), 0);
    lv_obj_align(lbl_battery_percent, LV_ALIGN_LEFT_MID, 155, 0); // Nằm sát ngay icon pin

    // ========== NHÓM WiFi ==========
    lbl_header_wifi_signal = lv_label_create(h);
    lv_label_set_text(lbl_header_wifi_signal, "0/4");
    lv_obj_set_width(lbl_header_wifi_signal, 22);
    lv_label_set_long_mode(lbl_header_wifi_signal, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_header_wifi_signal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_header_wifi_signal, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_header_wifi_signal, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_header_wifi_signal, LV_ALIGN_LEFT_MID, 185, 0);

    lbl_header_wifi_icon = lv_label_create(h);
    lv_label_set_text(lbl_header_wifi_icon, ICON_WIFI);
    lv_obj_set_style_text_font(lbl_header_wifi_icon, &lv_font_wifi_24, 0);
    lv_obj_set_style_text_color(lbl_header_wifi_icon, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_header_wifi_icon, LV_ALIGN_LEFT_MID, 210, 0);

    lbl_header_wifi_ssid = lv_label_create(h);
    lv_label_set_text(lbl_header_wifi_ssid, "OFF");
    lv_obj_set_width(lbl_header_wifi_ssid, 55);
    lv_obj_set_style_text_align(lbl_header_wifi_ssid, LV_TEXT_ALIGN_LEFT, 0); // SỬA LỖI: Căn TRÁI để chữ hiện sát icon Wifi
    lv_label_set_long_mode(lbl_header_wifi_ssid, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lbl_header_wifi_ssid, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_header_wifi_ssid, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_header_wifi_ssid, LV_ALIGN_LEFT_MID, 240, 0);
}

// --- 8. CÁC MÀN HÌNH (GIAO DIỆN CHÍNH) ---

// Global menu buttons array for custom navigation
static lv_obj_t *menu_buttons[8] = {NULL};
static int current_menu_idx = 0;
static const int kMenuCols = 4;
static const int kMenuRows = 2;

// Custom menu navigation handler
void menu_key_handler(lv_event_t *e)
{
    uint32_t key = lv_indev_get_key(lv_indev_get_act());
    Serial.printf("[MENU NAV] Key: %d, Current idx: %d\n", key, current_menu_idx);

    int new_idx = current_menu_idx;
    int row = current_menu_idx / kMenuCols; // 0 or 1
    int col = current_menu_idx % kMenuCols; // 0..3

    // Grid 4x2: [0][1][2][3]
    //           [4][5][6][7]
    if (key == LV_KEY_DOWN)
    {
        // Move down: row 0→1, row 1→0 (wrap)
        row = (row + 1) % kMenuRows;
        new_idx = row * kMenuCols + col;
    }
    else if (key == LV_KEY_UP)
    {
        // Move up: row 1→0, row 0→1 (wrap)
        row = (row - 1 + kMenuRows) % kMenuRows;
        new_idx = row * kMenuCols + col;
    }
    else if (key == LV_KEY_RIGHT)
    {
        // Move right: col 0→1→2→0 (wrap)
        col = (col + 1) % kMenuCols;
        new_idx = row * kMenuCols + col;
    }
    else if (key == LV_KEY_LEFT)
    {
        // Move left: col 2→1→0→2 (wrap)
        col = (col - 1 + kMenuCols) % kMenuCols;
        new_idx = row * kMenuCols + col;
    }

    if (new_idx != current_menu_idx && menu_buttons[new_idx])
    {
        current_menu_idx = new_idx;
        lv_group_focus_obj(menu_buttons[new_idx]);
        Serial.printf("[MENU NAV] Moved to idx: %d (row:%d, col:%d)\n", new_idx, row, col);
    }
}

// MÀN HÌNH MENU
void build_menu()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "MAIN MENU");

    // Grid Layout
    static lv_coord_t col_dsc[] = {72, 72, 72, 72, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {88, 88, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 312, 206);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lock_scroll(grid);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, 2, 0);
    lv_obj_set_style_pad_gap(grid, 3, 0);

    struct Item
    {
        const char *ico;
        const lv_font_t *font;
        uint32_t color;
        const char *txt;
        ScreenType type;
    };
    Item items[] = {
        {ICON_MONITOR, &lv_font_monitor_24, 0x00E5FF, "Monitor", SCR_MONITOR},
        {ICON_ECG, &lv_font_ecg_24, 0x00E676, "ECG", SCR_ECG},
        {ICON_HEART, &lv_font_heart_24, 0xFF1744, "HR/SpO2", SCR_SPO2},
        {ICON_TEMP, &lv_font_temp_24, 0xFF9800, "Temp", SCR_TEMP},
        {ICON_AMBIENT, &lv_font_ambient_24, 0x00B8D4, "Collect", SCR_COLLECTDATA},
        {ICON_LUNG, &lv_font_lung_24, 0x8BC34A, "MeasureAll", SCR_MEASUREALL},
        {ICON_WIFI, &lv_font_wifi_24, 0x2196F3, "Wifi", SCR_WIFI_SCAN},
        {ICON_CONFIG, &lv_font_config_24, 0xFFEB3B, "Config", SCR_CONFIG}};

    int itemCount = sizeof(items) / sizeof(Item);
    for (int i = 0; i < itemCount; i++)
    {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_add_style(btn, &style_panel, 0);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % kMenuCols, 1, LV_GRID_ALIGN_STRETCH, i / kMenuCols, 1);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        static auto cb = [](lv_event_t *e)
        {
            long t = (long)lv_event_get_user_data(e);
            Serial.printf("[CLICKED] Switching to screen: %d\n", t);
            ui_switch_screen((ScreenType)t);
        };
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)items[i].type);
        lv_obj_add_event_cb(btn, menu_key_handler, LV_EVENT_KEY, NULL);

        lv_obj_t *l1 = lv_label_create(btn);
        lv_label_set_text(l1, items[i].ico);
        lv_obj_set_style_text_font(l1, items[i].font, 0);
        lv_obj_set_style_text_color(l1, lv_color_hex(items[i].color), 0);

        // Debug icon
        Serial.printf("[ICON] btn[%d]: font=%p, text bytes: %02X %02X %02X\n",
                      i, items[i].font,
                      (uint8_t)items[i].ico[0],
                      (uint8_t)items[i].ico[1],
                      (uint8_t)items[i].ico[2]);

        lv_obj_t *l2 = lv_label_create(btn);
        lv_label_set_text(l2, items[i].txt);
        lv_obj_set_style_text_font(l2, &lv_font_montserrat_12, 0);

        Serial.printf("[MENU] Adding btn[%d] = %p to group\n", i, btn);
        if (i < 8)
        {
            menu_buttons[i] = btn;
        }
        lv_group_add_obj(input_group, btn);

        if (i == 0)
        {
            lv_group_focus_obj(btn);
            current_menu_idx = 0;
            Serial.printf("[MENU] Focused btn[0] = %p\n", btn);
        }
    }

    lv_group_set_editing(input_group, false);
    lv_group_set_wrap(input_group, true);

    // Debug info
    Serial.printf("[MENU] Created with %d objects\n", lv_group_get_obj_count(input_group));
    Serial.printf("[MENU] Focused obj: %p\n", lv_group_get_focused(input_group));
    Serial.printf("[MENU] Editing: %d, Wrap: %d\n", lv_group_get_editing(input_group), lv_group_get_wrap(input_group));

    switch_to_obj(scr, SCR_MENU);
}

static void collectdata_key_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY)
    {
        return;
    }

    uint32_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_ENTER)
    {
        g_collect_take_requested = true;
    }
    else if (key == LV_KEY_LEFT)
    {
        g_collect_id_minus_requested = true;
    }
    else if (key == LV_KEY_RIGHT)
    {
        g_collect_id_plus_requested = true;
    }
    else if (key == LV_KEY_UP)
    {
        g_collect_session_plus_requested = true;
    }
    else if (key == LV_KEY_DOWN)
    {
        g_collect_reset_requested = true;
    }
}

static void measureall_key_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY)
    {
        return;
    }

    uint32_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_ENTER)
    {
        g_measure_all_start_requested = true;
    }
}

// MÀN HÌNH MONITOR (2x2 GRID: HR | SpO2 / Body Temp | Env Temp)
void build_monitor()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "MONITOR DASH");

    // Grid 2x2
    static lv_coord_t col_dsc[] = {154, 154, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {96, 96, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 312, 200);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lock_scroll(grid);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 2, 0);

    // TOP LEFT: HR
    lv_obj_t *hr_box = lv_obj_create(grid);
    lv_obj_add_style(hr_box, &style_panel, 0);
    lv_obj_set_style_shadow_width(hr_box, 0, 0);
    lock_scroll(hr_box);
    lv_obj_set_grid_cell(hr_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(hr_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hr_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l_hr = lv_label_create(hr_box);
    lv_label_set_text(l_hr, "HEART RATE");
    lv_obj_set_style_text_color(l_hr, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(l_hr, &lv_font_montserrat_10, 0);

    lbl_hr_val = lv_label_create(hr_box);
    lv_label_set_text(lbl_hr_val, "--");
    lv_obj_set_style_text_font(lbl_hr_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_hr_val, lv_color_hex(0xFF1744), 0); // Bright red

    lv_obj_t *hr_unit = lv_label_create(hr_box);
    lv_label_set_text(hr_unit, "BPM");
    lv_obj_set_style_text_color(hr_unit, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(hr_unit, &lv_font_montserrat_10, 0);

    // TOP RIGHT: SpO2
    lv_obj_t *spo2_box = lv_obj_create(grid);
    lv_obj_add_style(spo2_box, &style_panel, 0);
    lv_obj_set_style_shadow_width(spo2_box, 0, 0);
    lock_scroll(spo2_box);
    lv_obj_set_grid_cell(spo2_box, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(spo2_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(spo2_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l_spo2 = lv_label_create(spo2_box);
    lv_label_set_text(l_spo2, "OXYGEN SAT");
    lv_obj_set_style_text_color(l_spo2, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(l_spo2, &lv_font_montserrat_10, 0);

    lbl_spo2_val = lv_label_create(spo2_box);
    lv_label_set_text(lbl_spo2_val, "--");
    lv_obj_set_style_text_font(lbl_spo2_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_spo2_val, lv_color_hex(0x00E5FF), 0); // Bright cyan

    lv_obj_t *spo2_unit = lv_label_create(spo2_box);
    lv_label_set_text(spo2_unit, "%");
    lv_obj_set_style_text_color(spo2_unit, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(spo2_unit, &lv_font_montserrat_10, 0);

    // BOTTOM WIDE: Body Temp | Env Temp
    lv_obj_t *temp_wide = lv_obj_create(grid);
    lv_obj_add_style(temp_wide, &style_panel, 0);
    lv_obj_set_style_shadow_width(temp_wide, 0, 0);
    lock_scroll(temp_wide);
    lv_obj_set_grid_cell(temp_wide, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_flex_flow(temp_wide, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_wide, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(temp_wide, 1, 0);

    // Body Temp (Left)
    lv_obj_t *body_cont = lv_obj_create(temp_wide);
    lv_obj_set_size(body_cont, 140, 75);
    lock_scroll(body_cont);
    lv_obj_set_style_bg_opa(body_cont, 0, 0);
    lv_obj_set_style_border_width(body_cont, 0, 0);
    lv_obj_set_flex_flow(body_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *body_lbl = lv_label_create(body_cont);
    lv_label_set_text(body_lbl, "BODY TEMP");
    lv_obj_set_style_text_color(body_lbl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_10, 0);

    lbl_temp_val = lv_label_create(body_cont);
    lv_label_set_text(lbl_temp_val, "--");
    lv_obj_set_style_text_font(lbl_temp_val, &lv_font_montserrat_44, 0);
    lv_obj_set_style_text_color(lbl_temp_val, lv_color_hex(0xFF9100), 0); // Bright orange

    // Divider
    lv_obj_t *divider = lv_obj_create(temp_wide);
    lv_obj_set_size(divider, 1, 65);
    lock_scroll(divider);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(divider, 0, 0);

    // Env Temp (Right)
    lv_obj_t *env_cont = lv_obj_create(temp_wide);
    lv_obj_set_size(env_cont, 140, 75);
    lock_scroll(env_cont);
    lv_obj_set_style_bg_opa(env_cont, 0, 0);
    lv_obj_set_style_border_width(env_cont, 0, 0);
    lv_obj_set_flex_flow(env_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(env_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *env_icon = lv_label_create(env_cont);
    lv_label_set_text(env_icon, ICON_AMBIENT);
    lv_obj_set_style_text_font(env_icon, &lv_font_ambient_24, 0);
    lv_obj_set_style_text_color(env_icon, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *env_lbl = lv_label_create(env_cont);
    lv_label_set_text(env_lbl, "ENV TEMP");
    lv_obj_set_style_text_color(env_lbl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(env_lbl, &lv_font_montserrat_10, 0);

    lbl_temp_env_val = lv_label_create(env_cont);
    lv_label_set_text(lbl_temp_env_val, "--");
    lv_obj_set_style_text_font(lbl_temp_env_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_temp_env_val, lv_color_hex(0xBBBBBB), 0);

    lbl_sensor_status = lv_label_create(scr);
    lv_label_set_text(lbl_sensor_status, "SENSOR STATUS");
    lv_obj_set_width(lbl_sensor_status, 300);
    lv_label_set_long_mode(lbl_sensor_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_sensor_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_sensor_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_sensor_status, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Dummy button để nhận input Back
    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(dummy, [](lv_event_t *e)
                        {
        if (lv_event_get_code(e) != LV_EVENT_KEY) {
            return;
        }

        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        if (key == LV_KEY_ENTER) {
            g_config_apply_requested = true;
            ui_set_config_status("APPLYING WIFI CONFIG...", 0xFFB300);
            Serial.println("[CONFIG_UI] ENTER pressed -> apply web config action");
        } }, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_MONITOR);
}

// MÀN HÌNH ECG (FULL CHART)
void build_ecg()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "ECG MONITOR");

    // Chart container
    lv_obj_t *chart_cont = lv_obj_create(scr);
    lv_obj_set_size(chart_cont, 320, 165);
    lv_obj_align(chart_cont, LV_ALIGN_TOP_MID, 0, 28);
    lock_scroll(chart_cont);
    lv_obj_set_style_bg_color(chart_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_cont, 1, 0);
    lv_obj_set_style_border_color(chart_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(chart_cont, 0, 0);

    chart_ecg = lv_chart_create(chart_cont);
    lv_obj_set_size(chart_ecg, 320, 165);
    lv_chart_set_type(chart_ecg, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_ecg, 160);
    lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_update_mode(chart_ecg, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_bg_color(chart_ecg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_ecg, 0, 0);
    lv_obj_set_style_size(chart_ecg, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart_ecg, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_ecg, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart_ecg, lv_color_hex(0x1a331a), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_ecg, 5, 14);
    ser_ecg = lv_chart_add_series(chart_ecg, lv_color_hex(0x00E676), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart_ecg, ser_ecg, 100);

    ecg_lbl_warning = lv_label_create(scr);
    lv_label_set_text(ecg_lbl_warning, "");
    lv_obj_set_style_text_color(ecg_lbl_warning, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_text_font(ecg_lbl_warning, &lv_font_montserrat_14, 0);
    lv_obj_align(ecg_lbl_warning, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(ecg_lbl_warning, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, 320, 47);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lock_scroll(bottom_bar);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x0a0000), 0);
    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);

    ecg_beat_dot = lv_obj_create(bottom_bar);
    lv_obj_set_size(ecg_beat_dot, 12, 12);
    lv_obj_set_style_radius(ecg_beat_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ecg_beat_dot, lv_color_hex(0x330000), 0);
    lv_obj_set_style_border_width(ecg_beat_dot, 0, 0);
    lv_obj_align(ecg_beat_dot, LV_ALIGN_LEFT_MID, 8, 0);

    lbl_hr_val = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_hr_val, "--");
    lv_obj_set_style_text_font(lbl_hr_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_hr_val, lv_color_hex(0xFF1744), 0);
    lv_obj_align(lbl_hr_val, LV_ALIGN_LEFT_MID, 28, 0);

    lv_obj_t *lbl_bpm = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_bpm, "BPM");
    lv_obj_set_style_text_color(lbl_bpm, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(lbl_bpm, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_bpm, LV_ALIGN_LEFT_MID, 120, 0);

    lbl_sensor_status = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_sensor_status, "LIVE");
    lv_obj_set_width(lbl_sensor_status, 64);
    lv_label_set_long_mode(lbl_sensor_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_sensor_status, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(0xFF1744), 0);
    lv_obj_set_style_text_font(lbl_sensor_status, &lv_font_montserrat_10, 0);
    lv_obj_align_to(lbl_sensor_status, lbl_bpm, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *lbl_info = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_info, "II  25mm/s");
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_info, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_ECG);
}

// MÀN HÌNH SPO2 (HR + SpO2 + WAVE)
void build_spo2()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "HR & SPO2");

    // Top Stats Row (2/3 height)
    lv_obj_t *stats_row = lv_obj_create(scr);
    lv_obj_set_size(stats_row, 320, 143);
    lv_obj_align(stats_row, LV_ALIGN_TOP_MID, 0, 30);
    lock_scroll(stats_row);
    lv_obj_set_style_bg_opa(stats_row, 0, 0);
    lv_obj_set_style_border_width(stats_row, 0, 0);
    lv_obj_set_style_pad_all(stats_row, 0, 0);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);

    // HR Column (Left)
    lv_obj_t *hr_col = lv_obj_create(stats_row);
    lv_obj_set_size(hr_col, 160, 143);
    lock_scroll(hr_col);
    lv_obj_set_style_bg_color(hr_col, lv_color_hex(0x0a0000), 0);
    lv_obj_set_style_border_width(hr_col, 0, 0);
    lv_obj_set_flex_flow(hr_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hr_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *hr_icon = lv_label_create(hr_col);
    lv_label_set_text(hr_icon, ICON_HEART);
    lv_obj_set_style_text_font(hr_icon, &lv_font_heart_24, 0);
    lv_obj_set_style_text_color(hr_icon, lv_color_hex(0xFF1744), 0);

    lv_obj_t *hr_lbl = lv_label_create(hr_col);
    lv_label_set_text(hr_lbl, "HEART RATE");
    lv_obj_set_style_text_color(hr_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(hr_lbl, &lv_font_montserrat_10, 0);

    lbl_hr_val = lv_label_create(hr_col);
    lv_label_set_text(lbl_hr_val, "--");
    lv_obj_set_style_text_font(lbl_hr_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_hr_val, lv_color_hex(0xFF1744), 0);

    lv_obj_t *hr_unit = lv_label_create(hr_col);
    lv_label_set_text(hr_unit, "BPM");
    lv_obj_set_style_text_color(hr_unit, lv_color_hex(0x999999), 0);

    // SpO2 Column (Right)
    lv_obj_t *spo2_col = lv_obj_create(stats_row);
    lv_obj_set_size(spo2_col, 160, 143);
    lock_scroll(spo2_col);
    lv_obj_set_style_bg_color(spo2_col, lv_color_hex(0x000a0a), 0);
    lv_obj_set_style_border_width(spo2_col, 0, 0);
    lv_obj_set_flex_flow(spo2_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(spo2_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *spo2_icon = lv_label_create(spo2_col);
    lv_label_set_text(spo2_icon, ICON_OXYGEN);
    lv_obj_set_style_text_font(spo2_icon, &lv_font_oxygen_24, 0);
    lv_obj_set_style_text_color(spo2_icon, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *spo2_lbl = lv_label_create(spo2_col);
    lv_label_set_text(spo2_lbl, "OXYGEN");
    lv_obj_set_style_text_color(spo2_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(spo2_lbl, &lv_font_montserrat_10, 0);

    lbl_spo2_val = lv_label_create(spo2_col);
    lv_label_set_text(lbl_spo2_val, "--");
    lv_obj_set_style_text_font(lbl_spo2_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_spo2_val, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *spo2_unit = lv_label_create(spo2_col);
    lv_label_set_text(spo2_unit, "%");
    lv_obj_set_style_text_color(spo2_unit, lv_color_hex(0x999999), 0);

    // Bottom Wave Row (1/3 height)
    lv_obj_t *wave_row = lv_obj_create(scr);
    lv_obj_set_size(wave_row, 320, 67);
    lv_obj_align(wave_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lock_scroll(wave_row);
    lv_obj_set_style_bg_color(wave_row, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(wave_row, 0, 0);
    lv_obj_set_style_pad_all(wave_row, 0, 0);

    lv_obj_t *wave_lbl = lv_label_create(wave_row);
    lv_label_set_text(wave_lbl, "PLETH WAVE");
    lv_obj_set_style_text_color(wave_lbl, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(wave_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(wave_lbl, LV_ALIGN_TOP_LEFT, 5, 2);

    chart_ecg = lv_chart_create(wave_row);
    lv_obj_set_size(chart_ecg, 320, 67);
    lv_chart_set_type(chart_ecg, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_ecg, 80);
    lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(chart_ecg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_ecg, 1, 0);
    lv_obj_set_style_border_color(chart_ecg, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_line_width(chart_ecg, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_ecg, 0, LV_PART_INDICATOR);
    // Grid lines
    lv_obj_set_style_line_color(chart_ecg, lv_color_hex(0x1a3333), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_ecg, 3, 8);
    ser_ecg = lv_chart_add_series(chart_ecg, lv_color_hex(0x00E5FF), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart_ecg, ser_ecg, 100);

    lbl_sensor_status = lv_label_create(wave_row);
    lv_label_set_text(lbl_sensor_status, "SENSOR STATUS");
    lv_obj_set_width(lbl_sensor_status, 190);
    lv_obj_set_style_text_align(lbl_sensor_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(lbl_sensor_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_sensor_status, LV_ALIGN_TOP_RIGHT, -5, 2);
    lv_obj_move_foreground(lbl_sensor_status);

    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_SPO2);
}

// MÀN HÌNH TEMP (BODY + AMBIENT)
void build_temp()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "THERMOMETER");

    // Distance (goc trai tren)
    lv_obj_t *dist_box = lv_obj_create(scr);
    lv_obj_set_size(dist_box, 90, 50);
    lv_obj_align(dist_box, LV_ALIGN_TOP_LEFT, 10, 32);
    lock_scroll(dist_box);
    lv_obj_add_style(dist_box, &style_panel, 0);
    lv_obj_set_style_pad_all(dist_box, 5, 0);
    lv_obj_set_flex_flow(dist_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dist_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *dist_lbl = lv_label_create(dist_box);
    lv_label_set_text(dist_lbl, "DIST");
    lv_obj_set_style_text_color(dist_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(dist_lbl, &lv_font_montserrat_10, 0);

    lbl_temp_dist_val = lv_label_create(dist_box);
    lv_label_set_text(lbl_temp_dist_val, "-- mm");
    lv_obj_set_style_text_color(lbl_temp_dist_val, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_temp_dist_val, &lv_font_montserrat_14, 0);

    // Ambient Temp (góc phải trên)
    lv_obj_t *ambient_box = lv_obj_create(scr);
    lv_obj_set_size(ambient_box, 90, 50);
    lv_obj_align(ambient_box, LV_ALIGN_TOP_RIGHT, -10, 32);
    lock_scroll(ambient_box);
    lv_obj_add_style(ambient_box, &style_panel, 0);
    lv_obj_set_style_pad_all(ambient_box, 5, 0);
    lv_obj_set_flex_flow(ambient_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ambient_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon + Label row
    lv_obj_t *amb_row = lv_obj_create(ambient_box);
    lv_obj_set_size(amb_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lock_scroll(amb_row);
    lv_obj_set_style_bg_opa(amb_row, 0, 0);
    lv_obj_set_style_border_width(amb_row, 0, 0);
    lv_obj_set_style_pad_all(amb_row, 0, 0);
    lv_obj_set_flex_flow(amb_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(amb_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *amb_icon = lv_label_create(amb_row);
    lv_label_set_text(amb_icon, ICON_AMBIENT);
    lv_obj_set_style_text_font(amb_icon, &lv_font_ambient_24, 0);
    lv_obj_set_style_text_color(amb_icon, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *amb_lbl = lv_label_create(amb_row);
    lv_label_set_text(amb_lbl, " AMB");
    lv_obj_set_style_text_color(amb_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(amb_lbl, &lv_font_montserrat_10, 0);

    lbl_temp_env_val = lv_label_create(ambient_box);
    lv_label_set_text(lbl_temp_env_val, "--");
    lv_obj_set_style_text_color(lbl_temp_env_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_temp_env_val, &lv_font_montserrat_14, 0);

    // Body Temp Circle (center)
    lv_obj_t *circle = lv_obj_create(scr);
    lv_obj_set_size(circle, 130, 130);
    lv_obj_center(circle);
    lock_scroll(circle);
    lv_obj_set_style_radius(circle, 65, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(circle, 4, 0);
    lv_obj_set_style_border_color(circle, lv_color_hex(0xFF9100), 0);
    lv_obj_set_flex_flow(circle, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(circle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_temp_val = lv_label_create(circle);
    lv_label_set_text(lbl_temp_val, "--");
    lv_obj_set_style_text_font(lbl_temp_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_temp_val, lv_color_hex(0xFF9100), 0);

    lv_obj_t *temp_unit = lv_label_create(circle);
    lv_label_set_text(temp_unit, "CELSIUS");
    lv_obj_set_style_text_color(temp_unit, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(temp_unit, &lv_font_montserrat_10, 0);

    // Status Bar ở dưới
    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, 320, 30);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lock_scroll(status_bar);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_outline_width(status_bar, 0, 0);
    lv_obj_set_style_shadow_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar, LV_SCROLLBAR_MODE_OFF);

    lbl_sensor_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_sensor_status, "TEMP STATUS");
    lv_obj_set_width(lbl_sensor_status, 312);
    lv_label_set_long_mode(lbl_sensor_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_sensor_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_sensor_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(0x888888), 0);
    lv_obj_center(lbl_sensor_status);

    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_TEMP);
}

// MÀN HÌNH COLLECT DATA (UI riêng)
void build_collectdata()
{
    clean_resources();
    ui_datacollector_init();

    lv_obj_t *dummy = lv_btn_create(ui_MonitorScreen);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(dummy, collectdata_key_handler, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(ui_MonitorScreen, SCR_COLLECTDATA);
}

// MÀN HÌNH MEASURE ALL
// Layout:
//  [Header 28px]
//  [HR | SpO2 | TEMP]  <- hàng ngang 3 ô, cao 58px
//  [   ECG waveform  ] <- ô to, cao 120px
//  [dist ... status]   <- footer 18px
void build_measureall()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "MEASURE ALL");

    // ── HÀNG TRÊN: 3 ô chỉ số ──────────────────────────────────
    // Mỗi ô rộng 102px, cao 58px, gap 3px, bắt đầu y=30 (dưới header)
    const lv_coord_t TOP_Y = 30;
    const lv_coord_t ROW_H = 76; // taller vitals cells (shrinks ECG status box below)
    const lv_coord_t CELL_W = 102;
    const lv_coord_t GAP = 3;
    const lv_coord_t LEFT_X = 4;

    struct
    {
        const char *title;
        lv_color_t color;
        lv_obj_t **out;
    } cells[3] = {
        {"HR", lv_color_hex(0xFF1744), &lbl_all_hr},
        {"SpO2", lv_color_hex(0x00E5FF), &lbl_all_spo2},
        {"TEMP", lv_color_hex(0xFF9800), &lbl_all_temp},
    };

    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *box = lv_obj_create(scr);
        lv_obj_add_style(box, &style_panel, 0);
        lv_obj_set_style_shadow_width(box, 0, 0);
        lv_obj_set_style_pad_all(box, 2, 0);
        lock_scroll(box);
        lv_obj_set_size(box, CELL_W, ROW_H);
        lv_obj_set_pos(box, LEFT_X + i * (CELL_W + GAP), TOP_Y);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *lbl_title = lv_label_create(box);
        lv_label_set_text(lbl_title, cells[i].title);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_10, 0);

        *cells[i].out = lv_label_create(box);
        lv_label_set_text(*cells[i].out, "--");
        lv_obj_set_style_text_font(*cells[i].out, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(*cells[i].out, cells[i].color, 0);
    }

    // ── Ô DƯỚI: ECG waveform to ────────────────────────────────
    // y = TOP_Y + ROW_H + GAP, cao = 240 - header(28) - ROW_H - GAP - footer(18) - gap*2
    const lv_coord_t ECG_Y = TOP_Y + ROW_H + GAP;
    const lv_coord_t ECG_W = 312;
    const lv_coord_t ECG_H = 240 - 28 - ROW_H - GAP * 2 - 18; // ~113px

    lv_obj_t *ecg_box = lv_obj_create(scr);
    lv_obj_add_style(ecg_box, &style_panel, 0);
    lv_obj_set_style_shadow_width(ecg_box, 0, 0);
    lv_obj_set_style_pad_all(ecg_box, 3, 0);
    lv_obj_set_style_bg_color(ecg_box, lv_color_hex(0x050505), 0);
    lock_scroll(ecg_box);
    lv_obj_set_size(ecg_box, ECG_W, ECG_H);
    lv_obj_set_pos(ecg_box, LEFT_X, ECG_Y);

    chart_ecg_mini = lv_chart_create(ecg_box);
    lv_obj_set_size(chart_ecg_mini, ECG_W - 6, ECG_H - 6);
    lv_obj_align(chart_ecg_mini, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart_ecg_mini, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_ecg_mini, 80);
    lv_chart_set_range(chart_ecg_mini, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_update_mode(chart_ecg_mini, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_bg_opa(chart_ecg_mini, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_ecg_mini, 0, 0);
    lv_obj_set_style_pad_all(chart_ecg_mini, 0, 0);
    lv_obj_set_style_size(chart_ecg_mini, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart_ecg_mini, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_color(chart_ecg_mini, lv_color_hex(0x12302A), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_ecg_mini, 4, 8);
    ser_ecg_mini = lv_chart_add_series(chart_ecg_mini, lv_color_hex(0x00E676), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart_ecg_mini, ser_ecg_mini, 100);

    lv_obj_t *ecg_label = lv_label_create(ecg_box);
    lv_label_set_text(ecg_label, "ECG DATA");
    lv_obj_set_style_text_color(ecg_label, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(ecg_label, &lv_font_montserrat_10, 0);
    lv_obj_align(ecg_label, LV_ALIGN_TOP_LEFT, 2, 1);

    all_ecg_beat_dot = lv_obj_create(ecg_box);
    lv_obj_set_size(all_ecg_beat_dot, 12, 12);
    lv_obj_set_style_radius(all_ecg_beat_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(all_ecg_beat_dot, lv_color_hex(0x113322), 0);
    lv_obj_set_style_border_width(all_ecg_beat_dot, 0, 0);
    lv_obj_align(all_ecg_beat_dot, LV_ALIGN_CENTER, -94, -12);

    lbl_all_ecg_state = lv_label_create(ecg_box);
    lv_label_set_text(lbl_all_ecg_state, "ECG WAIT");
    lv_obj_set_style_text_font(lbl_all_ecg_state, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(0xFFB300), 0);
    lv_obj_align(lbl_all_ecg_state, LV_ALIGN_CENTER, 14, -14);

    lbl_all_ecg_quality = lv_label_create(ecg_box);
    lv_label_set_text(lbl_all_ecg_quality, "MQTT OFF");
    lv_obj_set_style_text_font(lbl_all_ecg_quality, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_all_ecg_quality, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_all_ecg_quality, LV_ALIGN_CENTER, 0, 14);

    lbl_all_ecg_amp = lv_label_create(ecg_box);
    lv_label_set_text(lbl_all_ecg_amp, "FRAME READY");
    lv_obj_set_style_text_font(lbl_all_ecg_amp, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_all_ecg_amp, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_all_ecg_amp, LV_ALIGN_BOTTOM_MID, 0, -6);

    // ── FOOTER: dist (trái) + status (phải) ────────────────────
    const lv_coord_t FOOTER_Y = ECG_Y + ECG_H + GAP;

    lbl_all_dist = lv_label_create(scr);
    lv_label_set_text(lbl_all_dist, "DIST: -- mm");
    lv_obj_set_style_text_font(lbl_all_dist, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_all_dist, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(lbl_all_dist, LEFT_X + 2, FOOTER_Y);

    lbl_all_status = lv_label_create(scr);
    lv_label_set_text(lbl_all_status, "ENTER: MEASURE & SEND");
    lv_obj_set_width(lbl_all_status, 200);
    lv_label_set_long_mode(lbl_all_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_all_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(lbl_all_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_all_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(lbl_all_status, 116, FOOTER_Y);

    // dummy button để bắt key
    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(dummy, measureall_key_handler, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_MEASUREALL);
}

// MÀN HÌNH CONFIG (WEB INFO)
void build_config()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    lock_scroll(scr);
    create_header(scr, "SYSTEM");

    // Main container
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 300, 190);
    lv_obj_center(cont);
    lock_scroll(cont);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 15, 0);

    // Web Config section
    lv_obj_t *web_lbl = lv_label_create(cont);
    lv_label_set_text(web_lbl, "WEB CONFIG IP");
    lv_obj_set_style_text_color(web_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(web_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(web_lbl, LV_ALIGN_TOP_MID, 0, 10);

    // IP box with border
    lv_obj_t *ip_box = lv_obj_create(cont);
    lv_obj_set_size(ip_box, 240, 80);
    lv_obj_align(ip_box, LV_ALIGN_TOP_MID, 0, 35);
    lock_scroll(ip_box);
    lv_obj_set_style_bg_color(ip_box, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(ip_box, 2, 0);
    lv_obj_set_style_border_color(ip_box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(ip_box, 8, 0);
    lv_obj_set_flex_flow(ip_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ip_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // IP icon
    lv_obj_t *ip_icon = lv_label_create(ip_box);
    lv_label_set_text(ip_icon, ICON_IP_ADDRESS);
    lv_obj_set_style_text_font(ip_icon, &lv_font_ip_address_24, 0);
    lv_obj_set_style_text_color(ip_icon, lv_color_hex(0x00E5FF), 0);

    // IP address
    lbl_config_ip = lv_label_create(ip_box);
    lv_label_set_text(lbl_config_ip, "0.0.0.0");
    lv_obj_set_style_text_color(lbl_config_ip, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_config_ip, &lv_font_montserrat_24, 0);

    // Instructions
    lbl_config_instruction = lv_label_create(cont);
    lv_label_set_text(lbl_config_instruction, "Connect from PC browser");
    lv_obj_set_width(lbl_config_instruction, 280);
    lv_obj_set_style_text_align(lbl_config_instruction, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_config_instruction, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_config_instruction, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_config_instruction, LV_ALIGN_BOTTOM_MID, 0, -18);

    // Status indicator
    lbl_config_status = lv_label_create(cont);
    lv_label_set_text(lbl_config_status, "STATUS: INITIALIZING");
    lv_obj_set_width(lbl_config_status, 280);
    lv_obj_set_style_text_align(lbl_config_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_config_status, lv_color_hex(0xFFB300), 0);
    lv_obj_set_style_text_font(lbl_config_status, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_config_status, LV_ALIGN_BOTTOM_MID, 0, 6);

    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_CONFIG);
}

// MÀN HÌNH WIFI SCAN
void build_wifi_scan()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    create_header(scr, "SELECT WIFI");

    wifi_scan_list = lv_list_create(scr);
    lv_obj_set_size(wifi_scan_list, 320, 214);
    lv_obj_align(wifi_scan_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(wifi_scan_list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(wifi_scan_list, 0, 0);
    lv_obj_set_style_pad_row(wifi_scan_list, 5, 0);
    lv_obj_add_event_cb(wifi_scan_list, handle_back_key, LV_EVENT_KEY, NULL);
    // lv_group_add_obj(input_group, wifi_scan_list);
    // lv_group_focus_obj(wifi_scan_list);

    wifi_scan_spinner = lv_spinner_create(scr, 900, 70);
    lv_obj_set_size(wifi_scan_spinner, 34, 34);
    lv_obj_align(wifi_scan_spinner, LV_ALIGN_CENTER, 0, -12);

    wifi_scan_loading_lbl = lv_label_create(scr);
    lv_label_set_text(wifi_scan_loading_lbl, "Dang quet WiFi");
    lv_obj_set_style_text_color(wifi_scan_loading_lbl, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(wifi_scan_loading_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_scan_loading_lbl, LV_ALIGN_CENTER, 0, 22);

    g_scanned_count = 0;
    g_selected_wifi_ssid = "";
    wifi_scan_anim_tick = 0;
    wifi_scan_retry_count = 0;
    start_wifi_scan_async();
    wifi_scan_timer = lv_timer_create(wifi_scan_tick, 220, NULL);

    lv_group_set_editing(input_group, false);
    switch_to_obj(scr, SCR_WIFI_SCAN);
}

void start_wifi_scan_async()
{
    WiFi.scanDelete();
    int rc = WiFi.scanNetworks(true, true);
    wifi_scan_started_ms = millis();

    if (wifi_scan_loading_lbl)
    {
        if (rc == WIFI_SCAN_FAILED)
        {
            lv_label_set_text(wifi_scan_loading_lbl, "Khoi tao scan that bai, dang thu lai...");
        }
        else
        {
            lv_label_set_text(wifi_scan_loading_lbl, "Dang quet WiFi");
        }
    }
}

void wifi_scan_render_results()
{
    if (!wifi_scan_list)
    {
        return;
    }

    lv_obj_clean(wifi_scan_list);

    if (wifi_scan_spinner)
    {
        lv_obj_del(wifi_scan_spinner);
        wifi_scan_spinner = NULL;
    }
    if (wifi_scan_loading_lbl)
    {
        lv_obj_del(wifi_scan_loading_lbl);
        wifi_scan_loading_lbl = NULL;
    }

    if (g_scanned_count <= 0)
    {
        lv_obj_t *btn = lv_list_add_btn(wifi_scan_list, LV_SYMBOL_REFRESH, "Khong tim thay WiFi - Enter de quet lai");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00E5FF), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_add_event_cb(btn, [](lv_event_t *e)
                            { ui_switch_screen(SCR_WIFI_SCAN); }, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, handle_back_key, LV_EVENT_KEY, NULL);
        lv_group_add_obj(input_group, btn);
        lv_group_focus_obj(btn);
        return;
    }

    for (int i = 0; i < g_scanned_count; i++)
    {
        lv_obj_t *btn = lv_list_add_btn(wifi_scan_list, LV_SYMBOL_WIFI, g_scanned_ssids[i].c_str());
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00E5FF), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(btn, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x00E5FF), LV_STATE_FOCUSED);
        lv_obj_set_style_pad_all(btn, 10, 0);
        lv_obj_set_style_radius(btn, 5, 0);

        lv_obj_add_event_cb(btn, [](lv_event_t *e)
                            {
            int selected = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            if (selected >= 0 && selected < g_scanned_count) {
                g_selected_wifi_ssid = g_scanned_ssids[selected];
                Serial.printf("[WIFI_UI] Selected SSID: %s\n", g_selected_wifi_ssid.c_str());
                ui_switch_screen(SCR_WIFI_PASS);
            } }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(btn, handle_back_key, LV_EVENT_KEY, NULL);

        lv_group_add_obj(input_group, btn);
    }

    lv_obj_t *focused = lv_obj_get_child(wifi_scan_list, 0);
    if (focused)
    {
        lv_group_focus_obj(focused);
        lv_obj_add_event_cb(focused, handle_back_key, LV_EVENT_KEY, NULL);
    }
}

void wifi_scan_tick(lv_timer_t *timer)
{
    (void)timer;

    if (!wifi_scan_list)
    {
        return;
    }

    if (wifi_scan_loading_lbl)
    {
        wifi_scan_anim_tick++;
        uint8_t dotCount = static_cast<uint8_t>(wifi_scan_anim_tick % 4);
        char animText[32];
        snprintf(animText, sizeof(animText), "Dang quet WiFi%.*s", dotCount, "...");
        lv_label_set_text(wifi_scan_loading_lbl, animText);
    }

    int scanStatus = WiFi.scanComplete();
    if (scanStatus == WIFI_SCAN_RUNNING)
    {
        if (millis() - wifi_scan_started_ms > 10000)
        {
            if (wifi_scan_retry_count < 2)
            {
                wifi_scan_retry_count++;
                if (wifi_scan_loading_lbl)
                {
                    lv_label_set_text(wifi_scan_loading_lbl, "Scan timeout, dang thu lai...");
                }
                start_wifi_scan_async();
            }
            else
            {
                WiFi.scanDelete();
                scanStatus = 0;
            }
        }
    }

    if (scanStatus == WIFI_SCAN_RUNNING)
    {
        return;
    }

    if (scanStatus == WIFI_SCAN_FAILED)
    {
        if (wifi_scan_retry_count < 2)
        {
            wifi_scan_retry_count++;
            if (wifi_scan_loading_lbl)
            {
                lv_label_set_text(wifi_scan_loading_lbl, "Scan loi, dang thu lai...");
            }
            start_wifi_scan_async();
            return;
        }
        scanStatus = 0;
    }

    g_scanned_count = 0;
    if (scanStatus > 0)
    {
        for (int i = 0; i < scanStatus && g_scanned_count < 20; i++)
        {
            String ssid = WiFi.SSID(i);
            if (ssid.isEmpty())
            {
                continue;
            }
            g_scanned_ssids[g_scanned_count++] = ssid;
        }
    }

    WiFi.scanDelete();

    if (wifi_scan_timer)
    {
        lv_timer_del(wifi_scan_timer);
        wifi_scan_timer = NULL;
    }

    wifi_scan_render_results();
}

// MÀN HÌNH WIFI PASS
void build_wifi_pass()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    create_header(scr, "ENTER PASSWORD");

    if (g_selected_wifi_ssid.isEmpty())
    {
        g_selected_wifi_ssid = "(unknown)";
    }

    // Container for instruction and back button
    lv_obj_t *top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 320, 35);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // BACK button (left side)
    lv_obj_t *btn_back = lv_btn_create(top_bar);
    lv_obj_set_size(btn_back, 60, 25);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xFF1744), 0);
    lv_obj_set_style_radius(btn_back, 4, 0);

    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_back);

    // Add focus styling for back button
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xFF5722), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn_back, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn_back, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);

    // Add back button to group first (for navigation)
    lv_group_add_obj(input_group, btn_back);

    // Auto exit editing when back button is focused
    lv_obj_add_event_cb(btn_back, [](lv_event_t *e)
                        {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_FOCUSED) {
            Serial.println("[BACK] Focused - exit editing");
            lv_group_set_editing(input_group, false);
        } else if (code == LV_EVENT_CLICKED) {
            Serial.println("[WIFI_PASS] BACK button clicked");
            ui_switch_screen(SCR_WIFI_SCAN);
        } }, LV_EVENT_ALL, NULL);

    // Instruction label (right side)
    lv_obj_t *inst = lv_label_create(top_bar);
    lv_label_set_text(inst, "ENTER: connect");
    lv_obj_set_style_text_color(inst, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_10, 0);

    lbl_wifi_selected_ssid = lv_label_create(scr);
    lv_label_set_text_fmt(lbl_wifi_selected_ssid, "SSID: %s", g_selected_wifi_ssid.c_str());
    lv_obj_set_width(lbl_wifi_selected_ssid, 280);
    lv_label_set_long_mode(lbl_wifi_selected_ssid, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_wifi_selected_ssid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_wifi_selected_ssid, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_wifi_selected_ssid, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wifi_selected_ssid, LV_ALIGN_TOP_MID, 0, 45);

    ta_wifi_pass = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta_wifi_pass, true);
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_obj_set_width(ta_wifi_pass, 300);
    lv_obj_align(ta_wifi_pass, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(ta_wifi_pass, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(ta_wifi_pass, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(ta_wifi_pass, 2, 0);
    lv_obj_set_style_border_color(ta_wifi_pass, lv_color_hex(0x00E5FF), 0);
    // Show cursor
    lv_obj_set_style_anim_time(ta_wifi_pass, 400, LV_PART_CURSOR);
    lv_obj_set_style_bg_color(ta_wifi_pass, lv_color_hex(0x00E5FF), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ta_wifi_pass, 255, LV_PART_CURSOR);

    // Prevent textarea from auto-entering editing mode
    lv_obj_add_event_cb(ta_wifi_pass, [](lv_event_t *e)
                        {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_FOCUSED) {
            // When textarea gets focus, ensure we're not in editing mode
            Serial.println("[TEXTAREA] Focused - staying in nav mode");
            lv_group_set_editing(input_group, false);
        } }, LV_EVENT_FOCUSED, NULL);

    lv_group_add_obj(input_group, ta_wifi_pass);

    lbl_wifi_pass_status = lv_label_create(scr);
    lv_label_set_text(lbl_wifi_pass_status, "Press ENTER on keyboard to connect");
    lv_obj_set_width(lbl_wifi_pass_status, 300);
    lv_obj_set_style_text_align(lbl_wifi_pass_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_wifi_pass_status, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_wifi_pass_status, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_wifi_pass_status, LV_ALIGN_TOP_MID, 0, 122);

    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, ta_wifi_pass);
    lv_obj_set_size(kb, 320, 105);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xFFFFFF), 0);
    lv_group_add_obj(input_group, kb);

    // Auto enter editing when keyboard is focused, handle OK button
    lv_obj_add_event_cb(kb, [](lv_event_t *e)
                        {
        lv_obj_t *btn_back = (lv_obj_t *)lv_event_get_user_data(e);
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_FOCUSED) {
            Serial.println("[KEYBOARD] Focused - enter editing");
            lv_group_set_editing(input_group, true);
        } else if (code == LV_EVENT_READY) {
            const char *pass = ta_wifi_pass ? lv_textarea_get_text(ta_wifi_pass) : "";
            g_wifi_connect_ssid = g_selected_wifi_ssid;
            g_wifi_connect_pass = pass ? String(pass) : String("");
            g_wifi_connect_requested = true;

            Serial.printf("[WIFI_UI] Connect request for SSID: %s\n", g_wifi_connect_ssid.c_str());
            ui_set_wifi_connect_feedback("CONNECTING...", 0xFFB300);

            lv_group_set_editing(input_group, false);
            lv_group_focus_obj(btn_back);
        } else if (code == LV_EVENT_KEY) {
            uint32_t key = lv_indev_get_key(lv_indev_get_act());
            if (key == LV_KEY_ESC) {
                // ESC - Go back to WiFi scan
                Serial.println("[WIFI_PASS] ESC - Go back");
                ui_switch_screen(SCR_WIFI_SCAN);
            }
        } }, LV_EVENT_ALL, btn_back);

    lv_group_focus_obj(kb);
    lv_group_set_editing(input_group, true); // Enable editing for keyboard navigation

    switch_to_obj(scr, SCR_WIFI_PASS);
}

// --- 9. SWITCH SCREEN ---
void ui_switch_screen(ScreenType scr)
{
    switch (scr)
    {
    case SCR_BOOT:
        build_boot();
        break;
    case SCR_MENU:
        build_menu();
        break;
    case SCR_MONITOR:
        build_monitor();
        break;
    case SCR_ECG:
        build_ecg();
        break;
    case SCR_SPO2:
        build_spo2();
        break;
    case SCR_TEMP:
        build_temp();
        break;
    case SCR_COLLECTDATA:
        build_collectdata();
        break;
    case SCR_MEASUREALL:
        build_measureall();
        break;
    case SCR_WIFI_SCAN:
        build_wifi_scan();
        break;
    case SCR_WIFI_PASS:
        build_wifi_pass();
        break;
    case SCR_CONFIG:
        build_config();
        break;
    default:
        build_menu();
        break;
    }
}

// --- 10. INIT (CHẠY ĐẦU TIÊN) ---
void ui_init()
{
    Serial.println("Init UI...");
    tft.begin();
    tft.setRotation(3);

    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = ui_input_read;
    lv_indev_t *my_indev = lv_indev_drv_register(&indev_drv);

    input_group = lv_group_create();
    lv_indev_set_group(my_indev, input_group);

    Serial.println("[INIT] Input group created and linked to indev");
    Serial.printf("[INIT] Group ptr: %p, Indev ptr: %p\n", input_group, my_indev);

    init_styles();
    ui_switch_screen(SCR_BOOT);
}

// --- 11. UPDATE SENSORS ---
void ui_update_sensors(float temp, int hr, int spo2, int ecg_val)
{
    static lv_obj_t *lastHrObj = NULL;
    static lv_obj_t *lastSpo2Obj = NULL;
    static lv_obj_t *lastTempObj = NULL;
    static int lastHrValue = -9999;
    static int lastSpo2Value = -9999;
    static int lastTempDeci = -9999;

    if (lbl_hr_val)
    {
        if (lbl_hr_val != lastHrObj)
        {
            lastHrObj = lbl_hr_val;
            lastHrValue = -9999;
        }

        const int hrValue = (hr > 0) ? hr : -1;
        if (hrValue != lastHrValue)
        {
            if (hrValue > 0)
                lv_label_set_text_fmt(lbl_hr_val, "%d", hrValue);
            else
                lv_label_set_text(lbl_hr_val, "--");
            lastHrValue = hrValue;
        }
    }

    if (lbl_spo2_val)
    {
        if (lbl_spo2_val != lastSpo2Obj)
        {
            lastSpo2Obj = lbl_spo2_val;
            lastSpo2Value = -9999;
        }

        const int spo2Value = (spo2 > 0) ? spo2 : -1;
        if (spo2Value != lastSpo2Value)
        {
            if (spo2Value > 0)
                lv_label_set_text_fmt(lbl_spo2_val, "%d", spo2Value);
            else
                lv_label_set_text(lbl_spo2_val, "--");
            lastSpo2Value = spo2Value;
        }
    }

    if (lbl_temp_val)
    {
        if (lbl_temp_val != lastTempObj)
        {
            lastTempObj = lbl_temp_val;
            lastTempDeci = -9999;
        }

        const int tempDeci = (temp > 0.0f) ? static_cast<int>(temp * 10.0f + 0.5f) : -1;
        if (tempDeci != lastTempDeci)
        {
            if (tempDeci >= 0)
            {
                char tempText[12];
                snprintf(tempText, sizeof(tempText), "%d.%d", tempDeci / 10, tempDeci % 10);
                lv_label_set_text(lbl_temp_val, tempText);
            }
            else
                lv_label_set_text(lbl_temp_val, "--");
            lastTempDeci = tempDeci;
        }
    }

    if (chart_ecg && ser_ecg)
    {
        const int ecgMax = (current_screen_type == SCR_ECG) ? 200 : 100;
        if (ecg_val < 0)
            ecg_val = 0;
        if (ecg_val > ecgMax)
            ecg_val = ecgMax;
        lv_chart_set_next_value(chart_ecg, ser_ecg, ecg_val);
    }
}

void ui_update_ecg_live(float ecg_mv, int hr_bpm, bool leads_connected)
{
    if (current_screen_type != SCR_ECG || !chart_ecg || !ser_ecg)
    {
        return;
    }

    // FIX: Đơn giản hoá toàn bộ hàm này.
    // TRƯỚC: hàm có displayBaseline + displayFiltered + envelope + yScale riêng
    //        → lọc lần 3 sau ad8232.cpp và lcd.cpp, làm xẹp QRS nghiêm trọng.
    // SAU:   ecg_mv là filteredSignal (mV) đã sạch từ ad8232.cpp (HPF+LPF+notch).
    //        Chỉ cần 1 envelope để autoscale chart. Không baseline thêm.

    static unsigned long lastChartUpdate = 0;
    static float envelope = 120.0f; // bám biên độ peak để autoscale
    static bool needsReprime = true;
    static int lastValidHr = 0;
    static unsigned long lastValidHrMs = 0;
    const unsigned long chartUpdateMs = 4; // FIX: 8→4ms (250Hz chart = khớp ADC rate)

    if (!leads_connected)
    {
        needsReprime = true;
        envelope = 120.0f;

        if (ecg_lbl_warning)
        {
            lv_label_set_text(ecg_lbl_warning, "LEADS NOT CONNECTED");
            lv_obj_clear_flag(ecg_lbl_warning, LV_OBJ_FLAG_HIDDEN);
        }

        lv_chart_set_next_value(chart_ecg, ser_ecg, 100);

        if (lbl_hr_val)
        {
            lv_label_set_text(lbl_hr_val, "--");
        }

        if (ecg_beat_dot)
        {
            lv_obj_set_style_bg_color(ecg_beat_dot, lv_color_hex(0x330000), 0);
        }
        return;
    }

    if (ecg_lbl_warning)
    {
        lv_obj_add_flag(ecg_lbl_warning, LV_OBJ_FLAG_HIDDEN);
    }

    if (needsReprime)
    {
        envelope = fmaxf(fabsf(ecg_mv), 80.0f);
        // Reset slowBaseline về giá trị hiện tại để tránh transient khi reconnect leads
        // Dùng static pointer trick: slowBaseline là static trong block bên dưới,
        // reset nó qua biến helper để tránh trùng scope
        lastChartUpdate = millis();
        needsReprime = false;
    }

    if (millis() - lastChartUpdate >= chartUpdateMs)
    {
        lastChartUpdate = millis();

        // Reject baseline wander cực chậm (fc≈0.05Hz) — chỉ loại DC drift dài hạn,
        // không ảnh hưởng sóng P/QRS/T. α=0.9998 @ 250Hz → τ≈8 giây
        static float slowBaseline = 0.0f;
        slowBaseline = 0.9998f * slowBaseline + 0.0002f * ecg_mv;
        const float centered = ecg_mv - slowBaseline;

        // Autoscale envelope: bám nhanh khi peak vượt, giảm chậm khi yên tĩnh
        const float absVal = fabsf(centered);
        if (absVal > envelope)
            envelope = 0.22f * absVal + 0.78f * envelope;
        else
            envelope = 0.003f * absVal + 0.997f * envelope;

        // Clamp envelope [80, 700] mV — sàn 80 để QRS luôn rõ dù tín hiệu nhỏ
        if (envelope < 80.0f)
            envelope = 80.0f;
        if (envelope > 700.0f)
            envelope = 700.0f;

        // Normalize về [-1, +1], clamp ±92% để headroom 8 unit
        const float clamped = constrain(centered, -0.92f * envelope, 0.92f * envelope);
        const float normalized = clamped / envelope;

        // FIX NGƯỢC SÓNG: LVGL chart y=0 ở ĐÁY, y=200 ở ĐỈNH
        // R-peak ECG dương → cần nhô LÊN màn hình → chartY phải LỚN (gần 200)
        // Công thức cũ: 100 - normalized*95 → R-peak dương cho chartY nhỏ → đâm XUỐNG (sai)
        // Công thức mới: 100 + normalized*95 → R-peak dương cho chartY lớn → nhô LÊN (đúng)
        const int chartY = constrain((int)(100.0f + normalized * 92.0f), 0, 200);

        lv_chart_set_next_value(chart_ecg, ser_ecg, chartY);
    }

    if (lbl_hr_val)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
        {
            lastValidHr = hr_bpm;
            lastValidHrMs = millis();
            lv_label_set_text_fmt(lbl_hr_val, "%d", hr_bpm);
        }
        else if (lastValidHr > 0 && (millis() - lastValidHrMs) < 3500)
        {
            lv_label_set_text_fmt(lbl_hr_val, "%d", lastValidHr);
        }
        else
        {
            lv_label_set_text(lbl_hr_val, "--");
        }
    }

    if (ecg_beat_dot)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
        {
            unsigned long beatMs = 60000 / hr_bpm;
            unsigned long phase = millis() % beatMs;
            bool heartBright = (phase < 150);
            lv_obj_set_style_bg_color(ecg_beat_dot,
                                      heartBright ? lv_color_hex(0xFF1744) : lv_color_hex(0x330000), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(ecg_beat_dot, lv_color_hex(0x330000), 0);
        }
    }
}

void ui_update_ecg_lcd_point(int waveformY200, int hr_bpm, bool leads_connected)
{
    if (current_screen_type != SCR_ECG || !chart_ecg || !ser_ecg)
    {
        return;
    }

    static int lastValidHr = 0;
    static unsigned long lastValidHrMs = 0;

    if (!leads_connected)
    {
        if (ecg_lbl_warning)
        {
            lv_label_set_text(ecg_lbl_warning, "LEADS NOT CONNECTED");
            lv_obj_clear_flag(ecg_lbl_warning, LV_OBJ_FLAG_HIDDEN);
        }

        lv_chart_set_next_value(chart_ecg, ser_ecg, 100);

        if (lbl_hr_val)
        {
            lv_label_set_text(lbl_hr_val, "--");
        }

        if (ecg_beat_dot)
        {
            lv_obj_set_style_bg_color(ecg_beat_dot, lv_color_hex(0x330000), 0);
        }
        return;
    }

    if (ecg_lbl_warning)
    {
        lv_obj_add_flag(ecg_lbl_warning, LV_OBJ_FLAG_HIDDEN);
    }

    lv_chart_set_next_value(chart_ecg, ser_ecg, constrain(waveformY200, 0, 200));

    if (lbl_hr_val)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
        {
            lastValidHr = hr_bpm;
            lastValidHrMs = millis();
            lv_label_set_text_fmt(lbl_hr_val, "%d", hr_bpm);
        }
        else if (lastValidHr > 0 && (millis() - lastValidHrMs) < 3500)
        {
            lv_label_set_text_fmt(lbl_hr_val, "%d", lastValidHr);
        }
        else
        {
            lv_label_set_text(lbl_hr_val, "--");
        }
    }

    if (ecg_beat_dot)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
        {
            unsigned long beatMs = 60000 / hr_bpm;
            unsigned long phase = millis() % beatMs;
            bool heartBright = (phase < 150);
            lv_obj_set_style_bg_color(ecg_beat_dot,
                                      heartBright ? lv_color_hex(0xFF1744) : lv_color_hex(0x330000), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(ecg_beat_dot, lv_color_hex(0x330000), 0);
        }
    }
}

void ui_set_ambient_temp(float temp)
{
    static lv_obj_t *lastAmbientObj = NULL;
    static int lastAmbientDeci = -9999;

    if (!lbl_temp_env_val)
    {
        return;
    }

    if (lbl_temp_env_val != lastAmbientObj)
    {
        lastAmbientObj = lbl_temp_env_val;
        lastAmbientDeci = -9999;
    }

    const int ambientDeci = (temp > -100.0f) ? static_cast<int>(temp * 10.0f + (temp >= 0.0f ? 0.5f : -0.5f)) : -1;
    if (ambientDeci == lastAmbientDeci)
    {
        return;
    }

    if (ambientDeci >= -500)
    {
        const int absDeci = (ambientDeci < 0) ? -ambientDeci : ambientDeci;
        char ambientText[16];
        snprintf(ambientText,
                 sizeof(ambientText),
                 "%s%d.%d C",
                 (ambientDeci < 0) ? "-" : "",
                 absDeci / 10,
                 absDeci % 10);
        lv_label_set_text(lbl_temp_env_val, ambientText);
    }
    else
    {
        lv_label_set_text(lbl_temp_env_val, "--");
    }

    lastAmbientDeci = ambientDeci;
}

void ui_set_sensor_status(const char *msg, uint32_t colorHex)
{
    static lv_obj_t *lastObj = NULL;
    static uint32_t lastColor = 0;
    static String lastMsg = "";

    if (!lbl_sensor_status)
    {
        return;
    }

    if (lbl_sensor_status != lastObj)
    {
        lastObj = lbl_sensor_status;
        lastColor = 0;
        lastMsg = "";
        lv_obj_set_style_text_align(lbl_sensor_status, LV_TEXT_ALIGN_CENTER, 0);
    }

    String nextMsg = msg ? String(msg) : String("");

    if (colorHex != lastColor)
    {
        lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(colorHex), 0);
        lastColor = colorHex;
    }

    if (nextMsg != lastMsg)
    {
        lv_label_set_text(lbl_sensor_status, nextMsg.c_str());
        lastMsg = nextMsg;
    }

    lv_obj_move_foreground(lbl_sensor_status);
}

void ui_set_config_ip(const char *ip)
{
    if (!lbl_config_ip)
    {
        return;
    }
    lv_label_set_text(lbl_config_ip, ip ? ip : "0.0.0.0");
}

void ui_set_config_status(const char *msg, uint32_t colorHex)
{
    static lv_obj_t *lastObj = NULL;
    static uint32_t lastColor = 0;
    static String lastMsg = "";

    if (!lbl_config_status)
    {
        return;
    }

    if (lbl_config_status != lastObj)
    {
        lastObj = lbl_config_status;
        lastColor = 0;
        lastMsg = "";
    }

    String nextMsg = msg ? String(msg) : String("STATUS: UNKNOWN");

    if (colorHex != lastColor)
    {
        lv_obj_set_style_text_color(lbl_config_status, lv_color_hex(colorHex), 0);
        lastColor = colorHex;
    }

    if (nextMsg != lastMsg)
    {
        lv_label_set_text(lbl_config_status, nextMsg.c_str());
        lastMsg = nextMsg;
    }
}

void ui_set_config_instruction(const char *msg)
{
    static lv_obj_t *lastObj = NULL;
    static String lastMsg = "";

    if (!lbl_config_instruction)
    {
        return;
    }

    if (lbl_config_instruction != lastObj)
    {
        lastObj = lbl_config_instruction;
        lastMsg = "";
    }

    String nextMsg = msg ? String(msg) : String("");
    if (nextMsg == lastMsg)
    {
        return;
    }

    lv_label_set_text(lbl_config_instruction, nextMsg.c_str());
    lastMsg = nextMsg;
}

bool ui_consume_config_apply_request()
{
    bool requested = g_config_apply_requested;
    g_config_apply_requested = false;
    return requested;
}

bool ui_consume_wifi_connect_request(String &ssid, String &pass)
{
    if (!g_wifi_connect_requested)
    {
        return false;
    }

    ssid = g_wifi_connect_ssid;
    pass = g_wifi_connect_pass;
    g_wifi_connect_requested = false;
    g_wifi_connect_ssid = "";
    g_wifi_connect_pass = "";
    return true;
}

bool ui_consume_mqtt_send_toggle_request()
{
    bool requested = g_mqtt_send_toggle_requested;
    g_mqtt_send_toggle_requested = false;
    return requested;
}

void ui_set_wifi_connect_feedback(const char *msg, uint32_t colorHex)
{
    if (!lbl_wifi_pass_status)
    {
        return;
    }

    lv_obj_set_style_text_color(lbl_wifi_pass_status, lv_color_hex(colorHex), 0);
    lv_label_set_text(lbl_wifi_pass_status, msg ? msg : "");
}

void ui_set_header_wifi(const char *signalLevel, const char *ssid, uint32_t colorHex)
{
    static lv_obj_t *lastSsidObj = NULL;
    static uint32_t lastColor = 0xFFFFFFFF; // Set giá trị ảo ban đầu
    static String lastSignal = "";
    static String lastSsid = "";

    String nextSignal = signalLevel ? String(signalLevel) : String("0/4");
    String nextSsid = ssid ? String(ssid) : String("OFF");

    // Khi chuyển màn hình, con trỏ object thay đổi -> Bắt buộc Reset toàn bộ Cache
    if (lbl_header_wifi_ssid != lastSsidObj)
    {
        lastSsidObj = lbl_header_wifi_ssid;
        lastColor = 0xFFFFFFFF;
        lastSignal = "";
        lastSsid = "";
    }

    if (lbl_header_wifi_icon && colorHex != lastColor)
    {
        lv_obj_set_style_text_color(lbl_header_wifi_icon, lv_color_hex(colorHex), 0);
    }

    if (lbl_header_wifi_signal)
    {
        if (colorHex != lastColor)
            lv_obj_set_style_text_color(lbl_header_wifi_signal, lv_color_hex(colorHex), 0);
        if (nextSignal != lastSignal)
            lv_label_set_text(lbl_header_wifi_signal, nextSignal.c_str());
    }

    if (lbl_header_wifi_ssid)
    {
        if (colorHex != lastColor)
            lv_obj_set_style_text_color(lbl_header_wifi_ssid, lv_color_hex(colorHex), 0);
        if (nextSsid != lastSsid)
            lv_label_set_text(lbl_header_wifi_ssid, nextSsid.c_str());
    }

    lastColor = colorHex;
    lastSignal = nextSignal;
    lastSsid = nextSsid;
}

void ui_set_mqtt_status(const char *msg, uint32_t colorHex)
{
    static lv_obj_t *lastObj = NULL;
    static String lastMsg = "";
    static uint32_t lastColor = 0;

    if (!lbl_header_mqtt_status)
    {
        return;
    }

    if (lbl_header_mqtt_status != lastObj)
    {
        lastObj = lbl_header_mqtt_status;
        lastMsg = "";
        lastColor = 0;
    }

    String nextMsg = msg ? String(msg) : String("");
    if (nextMsg.isEmpty())
    {
        nextMsg = "-";
    }

    if (nextMsg != lastMsg)
    {
        lv_label_set_text(lbl_header_mqtt_status, nextMsg.c_str());
        lastMsg = nextMsg;
    }

    if (colorHex != lastColor)
    {
        lv_obj_set_style_text_color(lbl_header_mqtt_status, lv_color_hex(colorHex), 0);
        lastColor = colorHex;
    }
}

bool ui_consume_collect_take_request()
{
    if (!g_collect_take_requested)
    {
        return false;
    }
    g_collect_take_requested = false;
    return true;
}

bool ui_consume_collect_id_minus_request()
{
    if (!g_collect_id_minus_requested)
    {
        return false;
    }
    g_collect_id_minus_requested = false;
    return true;
}

bool ui_consume_collect_id_plus_request()
{
    if (!g_collect_id_plus_requested)
    {
        return false;
    }
    g_collect_id_plus_requested = false;
    return true;
}

bool ui_consume_collect_session_plus_request()
{
    if (!g_collect_session_plus_requested)
    {
        return false;
    }

    g_collect_session_plus_requested = false;
    return true;
}

bool ui_consume_collect_reset_request()
{
    if (!g_collect_reset_requested)
    {
        return false;
    }
    g_collect_reset_requested = false;
    return true;
}

bool ui_consume_measure_all_start_request()
{
    if (!g_measure_all_start_requested)
    {
        return false;
    }
    g_measure_all_start_requested = false;
    return true;
}

void ui_set_measure_all_values(float temp, int hr, int spo2, float ecg, float dist)
{
    if (lbl_all_temp)
    {
        char buf[16];
        if (temp < 0)
        {
            snprintf(buf, sizeof(buf), "--");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%.1f", temp);
        }
        lv_label_set_text(lbl_all_temp, buf);
    }

    if (lbl_all_hr)
    {
        char buf[16];
        if (hr <= 0)
        {
            snprintf(buf, sizeof(buf), "--");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", hr);
        }
        lv_label_set_text(lbl_all_hr, buf);
    }

    if (lbl_all_spo2)
    {
        char buf[16];
        if (spo2 <= 0)
        {
            snprintf(buf, sizeof(buf), "--");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", spo2);
        }
        lv_label_set_text(lbl_all_spo2, buf);
    }

    if (lbl_all_ecg)
    {
        char buf[16];
        if (ecg == 0.0f)
        {
            snprintf(buf, sizeof(buf), "--");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%.2f", ecg);
        }
        lv_label_set_text(lbl_all_ecg, buf);
    }

    if (lbl_all_dist)
    {
        char buf[24];
        if (dist >= 999.0f)
        {
            snprintf(buf, sizeof(buf), "DIST: -- mm");
        }
        else
        {
            snprintf(buf, sizeof(buf), "DIST: %.0f mm", dist);
        }
        lv_label_set_text(lbl_all_dist, buf);
    }
}

void ui_set_measure_all_status(const char *msg, uint32_t colorHex)
{
    if (!lbl_all_status)
    {
        return;
    }

    lv_label_set_text(lbl_all_status, msg ? msg : "");
    lv_obj_set_style_text_color(lbl_all_status, lv_color_hex(colorHex), 0);
}

void ui_update_measure_all_ecg_status(bool leadsOn,
                                      bool mqttSending,
                                      float ecgMv,
                                      int hrBpm,
                                      bool frameSentRecently,
                                      uint8_t framePoints,
                                      float frameP2pMv,
                                      uint8_t frameClipPct)
{
    if (current_screen_type != SCR_MEASUREALL)
    {
        return;
    }

    if (lbl_all_ecg_state)
    {
        lv_label_set_text(lbl_all_ecg_state, leadsOn ? "ECG DATA" : "ECG LEADS OFF");
        lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(leadsOn ? 0x00E676 : 0xFF5252), 0);
    }

    if (lbl_all_ecg_quality)
    {
        if (mqttSending)
        {
            if (frameSentRecently)
            {
                lv_label_set_text_fmt(lbl_all_ecg_quality, "SENT %upt C%u%%", framePoints, frameClipPct);
            }
            else
            {
                lv_label_set_text_fmt(lbl_all_ecg_quality, "MQTT ON C%u%%", frameClipPct);
            }
        }
        else
        {
            lv_label_set_text_fmt(lbl_all_ecg_quality, "MQTT OFF C%u%%", frameClipPct);
        }
        lv_obj_set_style_text_color(lbl_all_ecg_quality,
                                    lv_color_hex(frameSentRecently ? 0x00E676 : (mqttSending ? 0x00E5FF : 0xAAAAAA)),
                                    0);
    }

    if (lbl_all_ecg_amp)
    {
        if (leadsOn)
        {
            if (hrBpm > 0)
            {
                lv_label_set_text_fmt(lbl_all_ecg_amp, "P2P %.0fmV | HR %d", frameP2pMv, hrBpm);
            }
            else
            {
                lv_label_set_text_fmt(lbl_all_ecg_amp, "P2P %.0fmV | HR --", frameP2pMv);
            }
        }
        else
        {
            lv_label_set_text(lbl_all_ecg_amp, "WAITING SIGNAL");
        }
    }

    if (all_ecg_beat_dot)
    {
        lv_obj_set_style_bg_color(all_ecg_beat_dot,
                                  leadsOn ? lv_color_hex(0x00E676) : lv_color_hex(0x331111),
                                  0);
    }
}

void ui_update_measure_all_ecg_waveform(int waveformY200, bool leadsOn)
{
    if (current_screen_type != SCR_MEASUREALL || !chart_ecg_mini || !ser_ecg_mini)
    {
        return;
    }

    static uint8_t clipCount = 0;
    static uint8_t frameCount = 0;
    static uint8_t weakCount = 0;
    static uint8_t lastY = 100;
    static uint16_t motionSum = 0;
    static unsigned long beatFlashUntil = 0;
    static unsigned long lastStatusUpdate = 0;

    if (!leadsOn)
    {
        clipCount = 0;
        frameCount = 0;
        weakCount = 0;
        motionSum = 0;
        lastY = 100;

        lv_chart_set_next_value(chart_ecg_mini, ser_ecg_mini, 100);
        if (lbl_all_ecg_state)
        {
            lv_label_set_text(lbl_all_ecg_state, "ECG LEADS OFF");
            lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(0xFF5252), 0);
        }
        if (lbl_all_ecg_quality)
        {
            lv_label_set_text(lbl_all_ecg_quality, "Q: --");
            lv_obj_set_style_text_color(lbl_all_ecg_quality, lv_color_hex(0x888888), 0);
        }
        if (lbl_all_ecg_amp)
        {
            lv_label_set_text(lbl_all_ecg_amp, "AMP --");
        }
        if (all_ecg_beat_dot)
        {
            lv_obj_set_style_bg_color(all_ecg_beat_dot, lv_color_hex(0x331111), 0);
        }
        return;
    }

    const uint8_t y = static_cast<uint8_t>(constrain(waveformY200, 0, 200));
    lv_chart_set_next_value(chart_ecg_mini, ser_ecg_mini, y);

    const uint8_t centered = (y > 100) ? (y - 100) : (100 - y);
    const uint8_t delta = (y > lastY) ? (y - lastY) : (lastY - y);
    lastY = y;
    motionSum += delta;
    frameCount++;
    if (y <= 5 || y >= 195)
    {
        clipCount++;
    }
    if (centered < 4)
    {
        weakCount++;
    }
    if (centered > 42 || delta > 34)
    {
        beatFlashUntil = millis() + 90;
    }

    if (all_ecg_beat_dot)
    {
        lv_obj_set_style_bg_color(all_ecg_beat_dot,
                                  (millis() < beatFlashUntil) ? lv_color_hex(0x00E676) : lv_color_hex(0x113322), 0);
    }

    if (millis() - lastStatusUpdate < 250)
    {
        return;
    }
    lastStatusUpdate = millis();

    const uint8_t clipPct = frameCount > 0 ? (clipCount * 100 / frameCount) : 0;
    const uint8_t weakPct = frameCount > 0 ? (weakCount * 100 / frameCount) : 0;
    const uint8_t avgMotion = frameCount > 0 ? (motionSum / frameCount) : 0;
    const char *quality = "OK";
    uint32_t qualityColor = 0x00E676;

    if (clipPct > 30)
    {
        quality = "CLIP";
        qualityColor = 0xFF5252;
    }
    else if (weakPct > 75 || avgMotion < 2)
    {
        quality = "WEAK";
        qualityColor = 0xFFB300;
    }
    else if (clipPct > 12 || avgMotion > 45)
    {
        quality = "NOISY";
        qualityColor = 0xFFB300;
    }

    if (lbl_all_ecg_state)
    {
        lv_label_set_text(lbl_all_ecg_state, "ECG LIVE");
        lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(0x00E676), 0);
    }
    if (lbl_all_ecg_quality)
    {
        lv_label_set_text_fmt(lbl_all_ecg_quality, "Q: %s", quality);
        lv_obj_set_style_text_color(lbl_all_ecg_quality, lv_color_hex(qualityColor), 0);
    }
    if (lbl_all_ecg_amp)
    {
        lv_label_set_text_fmt(lbl_all_ecg_amp, "AMP %u/100", static_cast<unsigned>(centered));
    }

    clipCount = 0;
    frameCount = 0;
    weakCount = 0;
    motionSum = 0;
}

void ui_update_measureall_ecg(float ecg_mv, bool leads_connected)
{
    if (current_screen_type != SCR_MEASUREALL || !chart_ecg_mini || !ser_ecg_mini)
    {
        return;
    }

    static unsigned long lastUpdate = 0;
    static float displayBaseline = 0.0f;
    static float displayFiltered = 0.0f;
    static float envelope = 110.0f;
    static float yScale = 190.0f;
    static bool needsReprime = true;
    static uint8_t clipCount = 0;
    static uint8_t frameCount = 0;
    static unsigned long beatFlashUntil = 0;
    static unsigned long lastStatusUpdate = 0;

    if (!leads_connected)
    {
        needsReprime = true;
        clipCount = 0;
        frameCount = 0;
        if (lbl_all_ecg_state)
        {
            lv_label_set_text(lbl_all_ecg_state, "ECG LEADS OFF");
            lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(0xFF5252), 0);
        }
        if (lbl_all_ecg_quality)
        {
            lv_label_set_text(lbl_all_ecg_quality, "Q: --");
            lv_obj_set_style_text_color(lbl_all_ecg_quality, lv_color_hex(0x888888), 0);
        }
        if (lbl_all_ecg_amp)
        {
            lv_label_set_text(lbl_all_ecg_amp, "AMP --");
        }
        if (all_ecg_beat_dot)
        {
            lv_obj_set_style_bg_color(all_ecg_beat_dot, lv_color_hex(0x331111), 0);
        }
        lv_chart_set_next_value(chart_ecg_mini, ser_ecg_mini, 100);
        return;
    }

    if (needsReprime)
    {
        displayBaseline = ecg_mv;
        displayFiltered = 0.0f;
        envelope = 110.0f;
        yScale = 190.0f;
        clipCount = 0;
        frameCount = 0;
        beatFlashUntil = 0;
        lastUpdate = millis();
        needsReprime = false;
    }

    if (millis() - lastUpdate >= 8)
    {
        lastUpdate = millis();

        float baselineInput = constrain(ecg_mv, -90.0f, 90.0f);
        displayBaseline = 0.992f * displayBaseline + 0.008f * baselineInput;
        float centered = ecg_mv - displayBaseline;

        float delta = fabsf(centered - displayFiltered);
        float smoothAlpha = (delta > 40.0f) ? 0.28f : 0.58f;
        displayFiltered = smoothAlpha * displayFiltered + (1.0f - smoothAlpha) * centered;

        float absDisplay = fabsf(displayFiltered);
        if (absDisplay > envelope)
            envelope = 0.20f * absDisplay + 0.80f * envelope;
        else
            envelope = 0.005f * absDisplay + 0.995f * envelope;
        envelope = constrain(envelope, 70.0f, 300.0f);

        float targetScale = envelope * 1.70f;
        if (targetScale < 170.0f)
            targetScale = 170.0f;
        yScale = 0.97f * yScale + 0.03f * targetScale;
        yScale = constrain(yScale, 160.0f, 380.0f);

        float limited = constrain(displayFiltered, -0.95f * yScale, 0.95f * yScale);
        int ecgChart = (int)((limited / yScale + 1.0f) * 100.0f);
        ecgChart = constrain(ecgChart, 0, 200);

        lv_chart_set_next_value(chart_ecg_mini, ser_ecg_mini, ecgChart);

        frameCount++;
        if (ecgChart <= 8 || ecgChart >= 192)
        {
            clipCount++;
        }

        if (fabsf(displayFiltered) > 0.70f * envelope)
        {
            beatFlashUntil = millis() + 90;
        }

        if (all_ecg_beat_dot)
        {
            lv_obj_set_style_bg_color(all_ecg_beat_dot,
                                      (millis() < beatFlashUntil) ? lv_color_hex(0x00E676) : lv_color_hex(0x113322), 0);
        }

        if (millis() - lastStatusUpdate >= 250)
        {
            lastStatusUpdate = millis();
            const uint8_t clipPct = (frameCount > 0) ? (clipCount * 100 / frameCount) : 0;
            const char *quality = "OK";
            uint32_t qualityColor = 0x00E676;

            if (clipPct > 35 || envelope > 260.0f)
            {
                quality = "CLIP";
                qualityColor = 0xFF5252;
            }
            else if (envelope < 75.0f)
            {
                quality = "WEAK";
                qualityColor = 0xFFB300;
            }
            else if (clipPct > 15)
            {
                quality = "NOISY";
                qualityColor = 0xFFB300;
            }

            if (lbl_all_ecg_state)
            {
                lv_label_set_text(lbl_all_ecg_state, "ECG LIVE");
                lv_obj_set_style_text_color(lbl_all_ecg_state, lv_color_hex(0x00E676), 0);
            }
            if (lbl_all_ecg_quality)
            {
                lv_label_set_text_fmt(lbl_all_ecg_quality, "Q: %s", quality);
                lv_obj_set_style_text_color(lbl_all_ecg_quality, lv_color_hex(qualityColor), 0);
            }
            if (lbl_all_ecg_amp)
            {
                lv_label_set_text_fmt(lbl_all_ecg_amp, "AMP %.0fmV", envelope);
            }

            clipCount = 0;
            frameCount = 0;
        }
    }
}

void ui_set_temp_distance(float distMm)
{
    static lv_obj_t *lastObj = NULL;
    static int lastDistInt = -1;

    if (!lbl_temp_dist_val)
    {
        return;
    }

    if (lbl_temp_dist_val != lastObj)
    {
        lastObj = lbl_temp_dist_val;
        lastDistInt = -1;
    }

    const int distInt = (distMm >= 0.0f && distMm < 999.0f) ? static_cast<int>(distMm + 0.5f) : -1;
    if (distInt == lastDistInt)
    {
        return;
    }

    if (distInt >= 0)
    {
        char distText[16];
        snprintf(distText, sizeof(distText), "%d mm", distInt);
        lv_label_set_text(lbl_temp_dist_val, distText);
    }
    else
    {
        lv_label_set_text(lbl_temp_dist_val, "-- mm");
    }

    lastDistInt = distInt;
}

/**
 * @brief Cập nhật icon và % pin trên header.
 *
 * Màu icon thay đổi theo mức pin:
 *   >= 50%  → xanh lá   (0x00E676)
 *   >= 20%  → vàng cam  (0xFFB300)
 *    < 20%  → đỏ        (0xFF5252)
 * Khi đang sạc: icon chuyển xanh dương (0x29B6F6)
 * Khi TP5100 báo FULL: hiển thị FULL màu xanh lá
 *
 * @param percent  % pin từ INA219 (0–100)
 * @param charging true nếu TP5100 CHRG active
 * @param full     true nếu TP5100 STDBY/FULL active
 */
void ui_set_battery(uint8_t percent, bool charging, bool full)
{
    static lv_obj_t *lastObj = NULL;
    static uint8_t lastPercent = 255;
    static bool lastCharging = false;
    static bool lastFull = false;

    // Reset cache nếu đổi màn hình
    if (lbl_battery_percent != lastObj)
    {
        lastObj = lbl_battery_percent;
        lastPercent = 255;
        lastFull = false;
    }

    if (percent == lastPercent && charging == lastCharging && full == lastFull)
    {
        return;
    }
    lastPercent = percent;
    lastCharging = charging;
    lastFull = full;

    // QUY TẮC MÀU SẮC
    uint32_t color;
    if (full)
        color = 0x00E676; // Đầy pin -> Xanh lá
    else if (charging)
        color = 0x29B6F6; // Đang sạc -> Xanh dương
    else if (percent >= 50)
        color = 0x00E676; // Trên 50% -> Xanh lá
    else if (percent >= 20)
        color = 0xFFB300; // Trên 20% -> Vàng Cam
    else
        color = 0xFF5252; // Dưới 20% -> Đỏ

    // Cập nhật màu Icon
    if (lbl_battery_icon)
    {
        lv_obj_set_style_text_color(lbl_battery_icon, lv_color_hex(color), 0);
    }

    if (lbl_battery_charge_icon)
    {
        if (charging && !full)
        {
            lv_obj_clear_flag(lbl_battery_charge_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(lbl_battery_charge_icon, lv_color_hex(0x29B6F6), 0);
        }
        else
        {
            lv_obj_add_flag(lbl_battery_charge_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Cập nhật Text và màu Text (% pin)
    if (lbl_battery_percent)
    {
        char buf[8];
        if (full)
        {
            snprintf(buf, sizeof(buf), "FULL");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d%%", percent);
        }
        lv_label_set_text(lbl_battery_percent, buf);
        lv_obj_set_style_text_color(lbl_battery_percent, lv_color_hex(color), 0);
    }
}
