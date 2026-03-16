#include "ui.h"
#include "custom_icons.h"
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

// --- 3. KHAI BÁO PROTOTYPE (Tránh lỗi "Not Declared") ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void ui_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
void init_styles();
void build_menu();
void build_monitor();
void build_ecg();
void build_spo2();
void build_temp();
void build_config();
void build_wifi_scan();
void build_wifi_pass();
void draw_lungs_icon(lv_obj_t *parent, int16_t x, int16_t y, lv_color_t color);
void draw_thermometer_icon(lv_obj_t *parent, int16_t x, int16_t y, lv_color_t color);

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
        Serial.println("BTN: ENTER");
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PR;
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
    if (input_group)
        lv_group_remove_all_objs(input_group);
    lbl_hr_val = NULL;
    lbl_spo2_val = NULL;
    lbl_temp_val = NULL;
    chart_ecg = NULL;
    ser_ecg = NULL;
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
    lv_obj_set_style_bg_color(h, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(h, 1, 0);
    lv_obj_set_style_border_color(h, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *l = lv_label_create(h);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_color(l, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);

    // WiFi icon (custom)
    lv_obj_t *wifi = lv_label_create(h);
    lv_label_set_text(wifi, ICON_WIFI);
    lv_obj_set_style_text_font(wifi, &lv_font_wifi_24, 0);
    lv_obj_set_style_text_color(wifi, lv_color_hex(0x00E676), 0);
    lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -36, 0);

    // Battery icon (custom)
    lv_obj_t *bat = lv_label_create(h);
    lv_label_set_text(bat, ICON_BATTERY);
    lv_obj_set_style_text_font(bat, &lv_font_battery_24, 0);
    lv_obj_set_style_text_color(bat, lv_color_hex(0x00E676), 0);
    lv_obj_align(bat, LV_ALIGN_RIGHT_MID, -8, 0);
}

// --- 8. CÁC MÀN HÌNH (GIAO DIỆN CHÍNH) ---

// Global menu buttons array for custom navigation
static lv_obj_t *menu_buttons[6] = {NULL};
static int current_menu_idx = 0;

// Custom menu navigation handler
void menu_key_handler(lv_event_t *e)
{
    uint32_t key = lv_indev_get_key(lv_indev_get_act());
    Serial.printf("[MENU NAV] Key: %d, Current idx: %d\n", key, current_menu_idx);

    int new_idx = current_menu_idx;
    int row = current_menu_idx / 3; // 0 or 1
    int col = current_menu_idx % 3; // 0, 1, or 2

    // Grid 3x2: [0][1][2]
    //           [3][4][5]
    if (key == LV_KEY_DOWN)
    {
        // Move down: row 0→1, row 1→0 (wrap)
        row = (row + 1) % 2;
        new_idx = row * 3 + col;
    }
    else if (key == LV_KEY_UP)
    {
        // Move up: row 1→0, row 0→1 (wrap)
        row = (row - 1 + 2) % 2;
        new_idx = row * 3 + col;
    }
    else if (key == LV_KEY_RIGHT)
    {
        // Move right: col 0→1→2→0 (wrap)
        col = (col + 1) % 3;
        new_idx = row * 3 + col;
    }
    else if (key == LV_KEY_LEFT)
    {
        // Move left: col 2→1→0→2 (wrap)
        col = (col - 1 + 3) % 3;
        new_idx = row * 3 + col;
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
    create_header(scr, "MAIN MENU");

    // Grid Layout
    static lv_coord_t col_dsc[] = {98, 98, 98, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {80, 80, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 320, 210);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, 6, 0);
    lv_obj_set_style_pad_gap(grid, 5, 0);

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
        {ICON_WIFI, &lv_font_wifi_24, 0x2196F3, "Wifi", SCR_WIFI_SCAN},
        {ICON_CONFIG, &lv_font_config_24, 0xFFEB3B, "Config", SCR_CONFIG}};

    for (int i = 0; i < 6; i++)
    {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_add_style(btn, &style_panel, 0);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
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
        menu_buttons[i] = btn;
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

// MÀN HÌNH MONITOR (2x2 GRID: HR | SpO2 / Body Temp | Env Temp)
void build_monitor()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    create_header(scr, "MONITOR DASH");

    // Grid 2x2
    static lv_coord_t col_dsc[] = {160, 160, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {105, 105, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 320, 210);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 0, 0);

    // TOP LEFT: HR
    lv_obj_t *hr_box = lv_obj_create(grid);
    lv_obj_add_style(hr_box, &style_panel, 0);
    lv_obj_set_grid_cell(hr_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(hr_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hr_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l_hr = lv_label_create(hr_box);
    lv_label_set_text(l_hr, "HEART RATE");
    lv_obj_set_style_text_color(l_hr, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(l_hr, &lv_font_montserrat_10, 0);

    lv_obj_t *lbl_hr_val = lv_label_create(hr_box);
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
    lv_obj_set_grid_cell(spo2_box, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(spo2_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(spo2_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l_spo2 = lv_label_create(spo2_box);
    lv_label_set_text(l_spo2, "OXYGEN SAT");
    lv_obj_set_style_text_color(l_spo2, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(l_spo2, &lv_font_montserrat_10, 0);

    lv_obj_t *lbl_spo2_val = lv_label_create(spo2_box);
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
    lv_obj_set_grid_cell(temp_wide, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_flex_flow(temp_wide, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_wide, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Body Temp (Left)
    lv_obj_t *body_cont = lv_obj_create(temp_wide);
    lv_obj_set_size(body_cont, 140, 90);
    lv_obj_set_style_bg_opa(body_cont, 0, 0);
    lv_obj_set_style_border_width(body_cont, 0, 0);
    lv_obj_set_flex_flow(body_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *body_lbl = lv_label_create(body_cont);
    lv_label_set_text(body_lbl, "BODY TEMP");
    lv_obj_set_style_text_color(body_lbl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *lbl_temp_val = lv_label_create(body_cont);
    lv_label_set_text(lbl_temp_val, "--");
    lv_obj_set_style_text_font(lbl_temp_val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_temp_val, lv_color_hex(0xFF9100), 0); // Bright orange

    // Divider
    lv_obj_t *divider = lv_obj_create(temp_wide);
    lv_obj_set_size(divider, 1, 80);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(divider, 0, 0);

    // Env Temp (Right)
    lv_obj_t *env_cont = lv_obj_create(temp_wide);
    lv_obj_set_size(env_cont, 140, 90);
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

    lv_obj_t *env_val = lv_label_create(env_cont);
    lv_label_set_text(env_val, "28°C");
    lv_obj_set_style_text_font(env_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(env_val, lv_color_hex(0xBBBBBB), 0);

    // Dummy button để nhận input Back
    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
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
    create_header(scr, "ECG CHART");

    // Chart Container với grid background
    lv_obj_t *chart_cont = lv_obj_create(scr);
    lv_obj_set_size(chart_cont, 320, 188);
    lv_obj_align(chart_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(chart_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_cont, 1, 0);
    lv_obj_set_style_border_color(chart_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(chart_cont, 0, 0);

    chart_ecg = lv_chart_create(chart_cont);
    lv_obj_set_size(chart_ecg, 320, 188);
    lv_chart_set_type(chart_ecg, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_ecg, 150);
    lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(chart_ecg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_ecg, 0, 0);
    lv_obj_set_style_line_width(chart_ecg, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_ecg, 0, LV_PART_INDICATOR); // Hide point circles
    // Grid lines
    lv_obj_set_style_line_color(chart_ecg, lv_color_hex(0x1a331a), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_ecg, 5, 10);
    ser_ecg = lv_chart_add_series(chart_ecg, lv_color_hex(0x00E676), LV_CHART_AXIS_PRIMARY_Y);

    // Status Bar ở dưới
    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, 320, 22);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);

    lv_obj_t *lbl_live = lv_label_create(status_bar);
    lv_label_set_text(lbl_live, "LIVE LEAD I");
    lv_obj_set_style_text_color(lbl_live, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_live, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_live, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *lbl_speed = lv_label_create(status_bar);
    lv_label_set_text(lbl_speed, "25mm/s");
    lv_obj_set_style_text_color(lbl_speed, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(lbl_speed, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_speed, LV_ALIGN_RIGHT_MID, -10, 0);

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
    create_header(scr, "HR & SPO2");

    // Top Stats Row (2/3 height)
    lv_obj_t *stats_row = lv_obj_create(scr);
    lv_obj_set_size(stats_row, 320, 143);
    lv_obj_align(stats_row, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(stats_row, 0, 0);
    lv_obj_set_style_border_width(stats_row, 0, 0);
    lv_obj_set_style_pad_all(stats_row, 0, 0);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);

    // HR Column (Left)
    lv_obj_t *hr_col = lv_obj_create(stats_row);
    lv_obj_set_size(hr_col, 160, 143);
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
    lv_obj_set_style_bg_color(wave_row, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(wave_row, 0, 0);
    lv_obj_set_style_pad_all(wave_row, 0, 0);

    lv_obj_t *wave_lbl = lv_label_create(wave_row);
    lv_label_set_text(wave_lbl, "PLETH WAVE");
    lv_obj_set_style_text_color(wave_lbl, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(wave_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(wave_lbl, LV_ALIGN_TOP_LEFT, 5, 2);

    lv_obj_t *chart_pleth = lv_chart_create(wave_row);
    lv_obj_set_size(chart_pleth, 320, 67);
    lv_chart_set_type(chart_pleth, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_pleth, 80);
    lv_chart_set_range(chart_pleth, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(chart_pleth, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_pleth, 1, 0);
    lv_obj_set_style_border_color(chart_pleth, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_line_width(chart_pleth, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pleth, 0, LV_PART_INDICATOR);
    // Grid lines
    lv_obj_set_style_line_color(chart_pleth, lv_color_hex(0x1a3333), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_pleth, 3, 8);
    lv_chart_add_series(chart_pleth, lv_color_hex(0x00E5FF), LV_CHART_AXIS_PRIMARY_Y);

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
    create_header(scr, "THERMOMETER");

    // Ambient Temp (góc phải trên)
    lv_obj_t *ambient_box = lv_obj_create(scr);
    lv_obj_set_size(ambient_box, 90, 50);
    lv_obj_align(ambient_box, LV_ALIGN_TOP_RIGHT, -10, 32);
    lv_obj_add_style(ambient_box, &style_panel, 0);
    lv_obj_set_style_pad_all(ambient_box, 5, 0);
    lv_obj_set_flex_flow(ambient_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ambient_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon + Label row
    lv_obj_t *amb_row = lv_obj_create(ambient_box);
    lv_obj_set_size(amb_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
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

    lv_obj_t *amb_val = lv_label_create(ambient_box);
    lv_label_set_text(amb_val, "28°C");
    lv_obj_set_style_text_color(amb_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(amb_val, &lv_font_montserrat_14, 0);

    // Body Temp Circle (center)
    lv_obj_t *circle = lv_obj_create(scr);
    lv_obj_set_size(circle, 130, 130);
    lv_obj_center(circle);
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
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);

    lv_obj_t *status_txt = lv_label_create(status_bar);
    lv_label_set_text(status_txt, LV_SYMBOL_OK " STATUS: NORMAL");
    lv_obj_set_style_text_color(status_txt, lv_color_hex(0x00E676), 0);
    lv_obj_center(status_txt);

    lv_obj_t *dummy = lv_btn_create(scr);
    lv_obj_set_size(dummy, 1, 1);
    lv_obj_add_event_cb(dummy, handle_back_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(input_group, dummy);
    lv_group_focus_obj(dummy);
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_TEMP);
}

// MÀN HÌNH CONFIG (WEB INFO)
void build_config()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    create_header(scr, "SYSTEM");

    // Main container
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 300, 190);
    lv_obj_center(cont);
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
    lv_obj_t *ip_lbl = lv_label_create(ip_box);
    lv_label_set_text(ip_lbl, "192.168.4.1");
    lv_obj_set_style_text_color(ip_lbl, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_24, 0);

    // Instructions
    lv_obj_t *inst = lv_label_create(cont);
    lv_label_set_text(inst, "Connect to AP to configure");
    lv_obj_set_style_text_color(inst, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_10, 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -15);

    // Status indicator
    lv_obj_t *status = lv_label_create(cont);
    lv_label_set_text(status, "STATUS: NORMAL");
    lv_obj_set_style_text_color(status, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, 5);

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

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, 320, 214);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 5, 0);

    const char *networks[] = {"HOME_WIFI", "OFFICE_5G", "GUEST"};
    const char *signals[] = {"90ms", "70ms", "50ms"};
    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, networks[i]);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00E5FF), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(btn, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x00E5FF), LV_STATE_FOCUSED);
        lv_obj_set_style_pad_all(btn, 10, 0);
        lv_obj_set_style_radius(btn, 5, 0);

        // Add signal strength label
        lv_obj_t *sig_lbl = lv_label_create(btn);
        lv_label_set_text(sig_lbl, signals[i]);
        lv_obj_set_style_text_color(sig_lbl, lv_color_hex(0x00E5FF), 0);
        lv_obj_align(sig_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

        static auto cb = [](lv_event_t *e)
        { ui_switch_screen(SCR_WIFI_PASS); };
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

        lv_group_add_obj(input_group, btn);
    }

    lv_obj_t *f = lv_obj_get_child(list, 0);
    if (f)
    {
        lv_group_focus_obj(f);
        // Thêm event cho nút đầu tiên để xử lý ESC
        lv_obj_add_event_cb(f, handle_back_key, LV_EVENT_KEY, NULL);
    }
    lv_group_set_editing(input_group, false);

    switch_to_obj(scr, SCR_WIFI_SCAN);
}

// MÀN HÌNH WIFI PASS
void build_wifi_pass()
{
    clean_resources();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    create_header(scr, "ENTER PASSWORD");

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
    lv_label_set_text(inst, "Hold LEFT 1s=exit");
    lv_obj_set_style_text_color(inst, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_10, 0);

    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_obj_set_width(ta, 300);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x00E5FF), 0);
    // Show cursor
    lv_obj_set_style_anim_time(ta, 400, LV_PART_CURSOR);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x00E5FF), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ta, 255, LV_PART_CURSOR);

    // Prevent textarea from auto-entering editing mode
    lv_obj_add_event_cb(ta, [](lv_event_t *e)
                        {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_FOCUSED) {
            // When textarea gets focus, ensure we're not in editing mode
            Serial.println("[TEXTAREA] Focused - staying in nav mode");
            lv_group_set_editing(input_group, false);
        } }, LV_EVENT_FOCUSED, NULL);

    lv_group_add_obj(input_group, ta);

    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, ta);
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
            // OK button pressed - jump to back button
            Serial.println("[KEYBOARD] OK pressed - jump to back button");
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
    tft.setRotation(1);

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
    if (lbl_hr_val)
        lv_label_set_text_fmt(lbl_hr_val, "%d", hr);
    if (lbl_spo2_val)
        lv_label_set_text_fmt(lbl_spo2_val, "%d", spo2);
    if (lbl_temp_val)
        lv_label_set_text_fmt(lbl_temp_val, "%.1f C", temp);
    if (chart_ecg && ser_ecg)
        lv_chart_set_next_value(chart_ecg, ser_ecg, ecg_val);
}