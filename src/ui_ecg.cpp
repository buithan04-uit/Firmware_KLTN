/*
 * ===================================================================
 * UI_ECG.CPP - ECG STANDALONE UI IMPLEMENTATION
 * ===================================================================
 *
 * UI riêng cho ECG monitoring - KHÔNG ẢNH HƯỞNG ui.cpp hiện tại
 */

#include "ui_ecg.h"

// ==========================================
// HARDWARE CONFIG
// ==========================================
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_LEFT 25 // Back
#define BTN_RIGHT 26
#define BTN_ENTER 27

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// ==========================================
// LVGL VARIABLES
// ==========================================
static TFT_eSPI *tft_ptr = nullptr;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 20]; // 20 lines buffer

static lv_obj_t *ecg_screen = nullptr;
static lv_obj_t *chart_ecg = nullptr;
static lv_chart_series_t *ser_ecg = nullptr;
static lv_obj_t *lbl_hr_value = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_warning = nullptr;
static lv_obj_t *lbl_heart = nullptr; // Beat indicator dot

static uint32_t current_color = 0x00E676; // Green

// ==========================================
// STYLES
// ==========================================
static lv_style_t style_screen;
static lv_style_t style_header;
static lv_style_t style_chart;
static lv_style_t style_hr_big;

// ==========================================
// FORWARD DECLARATIONS
// ==========================================
static void ecg_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
static void init_styles();
static void build_ecg_screen();

// ==========================================
// DISPLAY FLUSH CALLBACK
// ==========================================
static void ecg_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    if (!tft_ptr)
        return;

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft_ptr->startWrite();
    tft_ptr->setAddrWindow(area->x1, area->y1, w, h);
    tft_ptr->pushColors((uint16_t *)&color_p->full, w * h, true);
    tft_ptr->endWrite();

    lv_disp_flush_ready(disp);
}

// ==========================================
// BUTTON INPUT CALLBACK
// ==========================================
void ecg_button_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static bool prev_up = HIGH;
    static bool prev_down = HIGH;
    static bool prev_left = HIGH;
    static bool prev_right = HIGH;
    static bool prev_enter = HIGH;

    bool curr_up = digitalRead(BTN_UP);
    bool curr_down = digitalRead(BTN_DOWN);
    bool curr_left = digitalRead(BTN_LEFT);
    bool curr_right = digitalRead(BTN_RIGHT);
    bool curr_enter = digitalRead(BTN_ENTER);

    data->state = LV_INDEV_STATE_REL;

    // Edge detection - chỉ trigger khi nhấn
    if (curr_enter == LOW && prev_enter == HIGH)
    {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PR;
        Serial.println("[ECG_UI] ENTER pressed");
    }
    else if (curr_up == LOW && prev_up == HIGH)
    {
        // UP: Zoom in / Increase sensitivity
        data->key = LV_KEY_UP;
        data->state = LV_INDEV_STATE_PR;
        Serial.println("[ECG_UI] UP pressed");
    }
    else if (curr_down == LOW && prev_down == HIGH)
    {
        // DOWN: Zoom out / Decrease sensitivity
        data->key = LV_KEY_DOWN;
        data->state = LV_INDEV_STATE_PR;
        Serial.println("[ECG_UI] DOWN pressed");
    }
    else if (curr_left == LOW && prev_left == HIGH)
    {
        // LEFT: Back to previous screen (nếu có)
        data->key = LV_KEY_LEFT;
        data->state = LV_INDEV_STATE_PR;
        Serial.println("[ECG_UI] LEFT pressed - Back");
    }
    else if (curr_right == LOW && prev_right == HIGH)
    {
        // RIGHT: Next screen (nếu có)
        data->key = LV_KEY_RIGHT;
        data->state = LV_INDEV_STATE_PR;
        Serial.println("[ECG_UI] RIGHT pressed");
    }

    prev_up = curr_up;
    prev_down = curr_down;
    prev_left = curr_left;
    prev_right = curr_right;
    prev_enter = curr_enter;
}

// ==========================================
// INITIALIZE STYLES
// ==========================================
static void init_styles()
{
    // Screen background: Black
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x000000));

    // Header style: Dark gray with border
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, lv_color_hex(0x1a1a1a));
    lv_style_set_border_width(&style_header, 1);
    lv_style_set_border_color(&style_header, lv_color_hex(0x404040));
    lv_style_set_pad_all(&style_header, 5);

    // Chart style: Medical green grid
    lv_style_init(&style_chart);
    lv_style_set_bg_color(&style_chart, lv_color_hex(0x000000));
    lv_style_set_border_width(&style_chart, 1);
    lv_style_set_border_color(&style_chart, lv_color_hex(0x1a331a));
    lv_style_set_line_width(&style_chart, 2);
    lv_style_set_line_color(&style_chart, lv_color_hex(0x1a331a));

    // HR big number style
    lv_style_init(&style_hr_big);
    lv_style_set_text_font(&style_hr_big, &lv_font_montserrat_48);
    lv_style_set_text_color(&style_hr_big, lv_color_hex(0xFF1744));
}

// ==========================================
// BUILD ECG SCREEN
// ==========================================
static void build_ecg_screen()
{
    // Tạo screen mới
    ecg_screen = lv_obj_create(NULL);
    lv_obj_add_style(ecg_screen, &style_screen, 0);

    // =========== HEADER (Top 25px) ===========
    lv_obj_t *header = lv_obj_create(ecg_screen);
    lv_obj_set_size(header, 320, 25);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_style(header, &style_header, 0);
    lv_obj_set_style_pad_all(header, 3, 0);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, "ECG MONITOR");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 5, 0);

    lbl_status = lv_label_create(header);
    lv_label_set_text(lbl_status, "LIVE");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF1744), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -5, 0);

    // =========== ECG CHART (Main area) ===========
    lv_obj_t *chart_container = lv_obj_create(ecg_screen);
    lv_obj_set_size(chart_container, 320, 165);
    lv_obj_align(chart_container, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(chart_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(chart_container, 1, 0);
    lv_obj_set_style_border_color(chart_container, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(chart_container, 0, 0);

    chart_ecg = lv_chart_create(chart_container);
    lv_obj_set_size(chart_ecg, 320, 165);
    lv_chart_set_type(chart_ecg, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_ecg, 80); // 80 points = 1.6s tại 50Hz, đỉnh tách xa để dễ đọc
    lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_update_mode(chart_ecg, LV_CHART_UPDATE_MODE_CIRCULAR); // Sweep mode
    lv_obj_add_style(chart_ecg, &style_chart, 0);
    lv_obj_set_style_size(chart_ecg, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart_ecg, 2, LV_PART_ITEMS); // Line thickness
    lv_chart_set_div_line_count(chart_ecg, 5, 12);

    // Add ECG series
    ser_ecg = lv_chart_add_series(chart_ecg, lv_color_hex(current_color), LV_CHART_AXIS_PRIMARY_Y);

    // =========== WARNING LABEL (Conditional) ===========
    lbl_warning = lv_label_create(ecg_screen);
    lv_label_set_text(lbl_warning, "");
    lv_obj_set_style_text_color(lbl_warning, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_text_font(lbl_warning, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_warning, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN); // Hidden by default

    // =========== BOTTOM STATUS BAR ===========
    lv_obj_t *bottom_bar = lv_obj_create(ecg_screen);
    lv_obj_set_size(bottom_bar, 320, 47);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x0a0000), 0);
    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);

    // Beat indicator (pulsing red dot)
    lbl_heart = lv_obj_create(bottom_bar);
    lv_obj_set_size(lbl_heart, 12, 12);
    lv_obj_set_style_radius(lbl_heart, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(lbl_heart, lv_color_hex(0x330000), 0);
    lv_obj_set_style_border_width(lbl_heart, 0, 0);
    lv_obj_align(lbl_heart, LV_ALIGN_LEFT_MID, 8, 0);

    // HR Value (Big red number)
    lbl_hr_value = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_hr_value, "--");
    lv_obj_add_style(lbl_hr_value, &style_hr_big, 0);
    lv_obj_align(lbl_hr_value, LV_ALIGN_LEFT_MID, 28, 0);

    // BPM unit
    lv_obj_t *lbl_bpm = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_bpm, "BPM");
    lv_obj_set_style_text_color(lbl_bpm, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(lbl_bpm, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_bpm, LV_ALIGN_LEFT_MID, 120, 0);

    // Lead + Speed info (right side)
    lv_obj_t *lbl_info = lv_label_create(bottom_bar);
    lv_label_set_text(lbl_info, "II  50mm/s");
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_info, LV_ALIGN_RIGHT_MID, -10, 0);

    // Load screen
    lv_scr_load(ecg_screen);
}

// ==========================================
// PUBLIC FUNCTIONS
// ==========================================

void ecg_ui_init(TFT_eSPI *tft_instance)
{
    Serial.println("[ECG_UI] Initializing...");

    tft_ptr = tft_instance;

    // Setup buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);

    // Initialize LVGL
    lv_init();

    // Setup display buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 20);

    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = ecg_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = ecg_button_read;
    lv_indev_drv_register(&indev_drv);

    // Initialize styles
    init_styles();

    // Build ECG screen
    build_ecg_screen();

    Serial.println("[ECG_UI] ✓ Initialized successfully");
}

void ecg_ui_update(float ecg_mV, int hr_bpm, bool leads_connected)
{
    if (!chart_ecg || !ser_ecg)
        return;

    // Check lead connection
    if (!leads_connected)
    {
        // Show warning
        if (lbl_warning)
        {
            lv_label_set_text(lbl_warning, "LEADS NOT CONNECTED");
            lv_obj_clear_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
        }

        // Set baseline
        lv_chart_set_next_value(chart_ecg, ser_ecg, 100); // Center line

        // Update HR to "--"
        if (lbl_hr_value)
            lv_label_set_text(lbl_hr_value, "--");

        // Dim beat indicator
        if (lbl_heart)
            lv_obj_set_style_bg_color(lbl_heart, lv_color_hex(0x330000), 0);

        return;
    }

    // Hide warning
    if (lbl_warning)
        lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);

    // ========== CHART UPDATE (time-based 50Hz) ==========
    // Downsample thông minh: trung bình cửa sổ + một phần peak để giữ R-wave
    static float windowPeak = 0;
    static float windowSum = 0;
    static int windowCount = 0;
    static bool windowHasData = false;
    static unsigned long lastChartUpdate = 0;

    // Track peak (giữ R-waves không bị mất)
    if (!windowHasData || fabsf(ecg_mV) > fabsf(windowPeak))
    {
        windowPeak = ecg_mV;
        windowHasData = true;
    }
    windowSum += ecg_mV;
    windowCount++;

    if (millis() - lastChartUpdate >= 20) // 50Hz chart update
    {
        lastChartUpdate = millis();

        float windowAvg = (windowCount > 0) ? (windowSum / windowCount) : 0.0f;
        float displayValue = 0.75f * windowAvg + 0.25f * windowPeak;

        // Fixed clinical-like scale để hình dạng ổn định, không co giãn giả
        const float yScale = 320.0f;

        // Normalize: ±yScale → 0-200 chart units
        int ecg_chart = (int)((displayValue / yScale + 1.0f) * 100.0f);
        ecg_chart = constrain(ecg_chart, 0, 200);

        lv_chart_set_next_value(chart_ecg, ser_ecg, ecg_chart);

        windowPeak = 0;
        windowSum = 0;
        windowCount = 0;
        windowHasData = false;
    }

    // ========== HEART RATE DISPLAY ==========
    if (lbl_hr_value)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
            lv_label_set_text_fmt(lbl_hr_value, "%d", hr_bpm);
        else
            lv_label_set_text(lbl_hr_value, "--");
    }

    // ========== BEAT ANIMATION (pulsing red dot) ==========
    if (lbl_heart)
    {
        if (hr_bpm > 30 && hr_bpm < 220)
        {
            // Nhấp nháy dot theo nhịp tim
            unsigned long beatMs = 60000 / hr_bpm;
            unsigned long phase = millis() % beatMs;
            bool heartBright = (phase < 150); // Flash 150ms mỗi nhịp
            lv_obj_set_style_bg_color(lbl_heart,
                                      heartBright ? lv_color_hex(0xFF1744) : lv_color_hex(0x330000), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(lbl_heart, lv_color_hex(0x330000), 0);
        }
    }
}

void ecg_ui_tick()
{
    lv_timer_handler();
}

void ecg_ui_reset_chart()
{
    if (!chart_ecg || !ser_ecg)
        return;

    // Clear all points
    lv_chart_set_all_value(chart_ecg, ser_ecg, 50); // Baseline at 50
    Serial.println("[ECG_UI] Chart reset");
}

void ecg_ui_show_grid(bool show)
{
    if (!chart_ecg)
        return;

    if (show)
    {
        lv_chart_set_div_line_count(chart_ecg, 5, 10);
    }
    else
    {
        lv_chart_set_div_line_count(chart_ecg, 0, 0);
    }
}

void ecg_ui_set_waveform_color(uint32_t color_hex)
{
    if (!chart_ecg || !ser_ecg)
        return;

    current_color = color_hex;
    lv_chart_set_series_color(chart_ecg, ser_ecg, lv_color_hex(color_hex));
    Serial.printf("[ECG_UI] Waveform color changed to 0x%06X\n", color_hex);
}
