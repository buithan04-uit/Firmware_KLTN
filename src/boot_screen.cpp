/*
 * ================================================================
 * BOOT SCREEN v3 — Redesigned for 320×240
 * ================================================================
 *
 *  ┌──────────────────────────────────────────────────────────┐  y=0
 *  │ [⊕]  │  B I O M O N I T O R          v2.0              │  Header 42px
 *  │      │  ECG · SpO2 · TEMP · IR                          │
 *  ├──────────────────────────────────────────────────────────┤  y=46
 *  │  ECG     AD8232          │  PPG     MAX30102             │
 *  │  ECG Signal · 250Hz      │  HR · SpO2               [OK]│  60px tall
 *  │  ~~~\/\/\/~~~       [OK] │  ~~~o~~~                     │
 *  ├──────────────────────────┼───────────────────────────────┤  y=109
 *  │  TEMP IR  MLX90614       │  WiFi    Network              │
 *  │  Non-contact Temp        │  192.168.1.7             [OK]│  60px tall
 *  │  ⊙             [RETRY]  │  ~~~wifi~~~                  │
 *  ├──────────────────────────┴───────────────────────────────┤  y=172
 *  │           TOF DIST   VL53L0X            [OK]            │  38px
 *  ├──────────────────────────────────────────────────────────┤  y=214
 *  │  ██████████████████░░░░░░░░░   8px progress bar         │  y=207
 *  │  ESP32_C0E8FC                                Heap:168KB │  y=219 (16px)
 *  └──────────────────────────────────────────────────────────┘  y=240
 *
 * THAY ĐỔI SO VỚI v2:
 *   - Logo y tế mới: vòng tròn + thánh giá + sóng ECG (SVG-style bằng LVGL)
 *   - Header cao hơn (42px), title font 16, tag font 10 → dễ đọc
 *   - Card name font 12 (tăng từ 12), card tag font 10 có letter-spacing
 *   - Loại bỏ card row 3 riêng biệt → TOF dùng full-width bar 38px
 *   - Badge to hơn: 48×16 thay vì 44×15 → chữ không bị tràn
 *   - Info bar + progress nằm dưới cùng, không bị đè nhau
 *   - Tất cả fonts có sẵn: Montserrat 10/12/16
 *
 * Font yêu cầu trong lv_conf.h:
 *   #define LV_FONT_MONTSERRAT_10  1
 *   #define LV_FONT_MONTSERRAT_12  1
 *   #define LV_FONT_MONTSERRAT_14  1   ← MỚI cần thêm
 *   #define LV_FONT_MONTSERRAT_16  1
 * ================================================================
 */

#include "boot_screen.h"
#include <lvgl.h>
#include <esp_system.h>

// ── Widget refs ──────────────────────────────────────────────────
static lv_obj_t *g_boot_scr = NULL;
static lv_obj_t *g_progress_fill = NULL;
static lv_obj_t *g_lbl_status = NULL;
static lv_obj_t *g_lbl_wifi_ip = NULL;
static lv_obj_t *g_lbl_heap = NULL;
static lv_obj_t *g_sensor_badge[BOOT_SENSOR_COUNT];
static lv_obj_t *g_sensor_status[BOOT_SENSOR_COUNT];

// ── Layout constants (320×240) ───────────────────────────────────
#define SCR_W 320
#define SCR_H 240

// Header
#define HDR_H 42
#define LOGO_W 52

// Card grid
#define CGAP 3                             // gap giữa cards
#define CPAD 4                             // padding cạnh màn hình
#define CW ((SCR_W - CPAD * 2 - CGAP) / 2) // 156
#define CH_AB 60                           // chiều cao card hàng 1 & 2
#define ROW1_Y (HDR_H + CGAP)              // 46
#define ROW2_Y (ROW1_Y + CH_AB + CGAP)     // 109
// TOF row: full-width, shorter
#define TOF_H 35
#define TOF_Y (ROW2_Y + CH_AB + CGAP) // 172

// Info / progress
#define PROG_Y (TOF_Y + TOF_H + 3) // 210
#define PROG_H 7
#define INFO_Y (PROG_Y + PROG_H + 2) // 219
#define INFO_H 18
#define PROG_W (SCR_W - CPAD * 2)

// ── Palette ──────────────────────────────────────────────────────
#define C_BG 0x050D0D
#define C_HDR 0x001820
#define C_HDR_LINE 0x003344
#define C_CARD 0x0B1818
#define C_CARD_BDR 0x1A3030
#define C_CARD_ACCENT 0x00B8D4 // left accent bar
#define C_TITLE 0x00E5FF
#define C_SUBTITLE 0x2E5060
#define C_DIM 0x3A5560
#define C_MID 0x607D8B
#define C_TAG 0x00B8D4
#define C_NAME 0xB0C8D0
#define C_DETAIL 0x4A6870
#define C_GRID 0x060F06
// Sensor status
#define C_OK_BG 0x003320
#define C_OK_FG 0x00E676
#define C_RETRY_BG 0x2A1C00
#define C_RETRY_FG 0xFFB300
#define C_FAIL_BG 0x2A0000
#define C_FAIL_FG 0xFF5252
#define C_WAIT_BG 0x0D1E1E
#define C_WAIT_FG 0x2A4050
// ECG/PPG waveform colors
#define C_ECG_WAVE 0xFF1744
#define C_PPG_WAVE 0x00E5FF
// Progress
#define C_PROG_TRACK 0x0A1A1A
#define C_PROG_FILL 0x00B8D4

// ── Helper: rectangle ───────────────────────────────────────────
static lv_obj_t *mk_rect(lv_obj_t *p,
                         int16_t x, int16_t y,
                         int16_t w, int16_t h,
                         uint32_t col, uint8_t r)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    return o;
}

// ── Helper: label ────────────────────────────────────────────────
static lv_obj_t *mk_label(lv_obj_t *p,
                          const char *txt,
                          const lv_font_t *font,
                          uint32_t col,
                          int16_t x, int16_t y)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

// ── Medical logo (top-left, 52×42) ──────────────────────────────
// Vẽ bằng LVGL shapes: vòng tròn + thánh giá + sóng ECG nhỏ
static void draw_medical_logo(lv_obj_t *parent, int16_t cx, int16_t cy)
{
    // Vòng tròn ngoài (outline)
    lv_obj_t *ring = lv_obj_create(parent);
    int16_t R = 15;
    lv_obj_set_pos(ring, cx - R, cy - R);
    lv_obj_set_size(ring, R * 2, R * 2);
    lv_obj_set_style_bg_color(ring, lv_color_hex(0x001830), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(C_CARD_ACCENT), 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_radius(ring, R, 0); // full circle
    lv_obj_set_style_pad_all(ring, 0, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ring, LV_SCROLLBAR_MODE_OFF);

    // Thánh giá dọc
    lv_obj_t *cross_v = mk_rect(parent, cx - 3, cy - 10, 6, 20, 0x001F40, 1);
    lv_obj_set_style_border_color(cross_v, lv_color_hex(0x005099), 0);
    lv_obj_set_style_border_width(cross_v, 1, 0);

    // Thánh giá ngang
    lv_obj_t *cross_h = mk_rect(parent, cx - 10, cy - 3, 20, 6, 0x001F40, 1);
    lv_obj_set_style_border_color(cross_h, lv_color_hex(0x005099), 0);
    lv_obj_set_style_border_width(cross_h, 1, 0);

    // Sóng ECG nhỏ chạy ngang qua thánh giá
    static lv_point_t ecg_pts[10];
    const int16_t dx[] = {-13, -8, -5, -3, 0, 2, 5, 7, 11, 13};
    const int16_t dy[] = {0, 0, 0, -7, 9, -7, 0, 0, 0, 0};
    for (int i = 0; i < 10; i++)
    {
        ecg_pts[i].x = cx + dx[i];
        ecg_pts[i].y = cy + dy[i];
    }
    lv_obj_t *wave = lv_line_create(parent);
    lv_line_set_points(wave, ecg_pts, 10);
    lv_obj_set_style_line_color(wave, lv_color_hex(C_ECG_WAVE), 0);
    lv_obj_set_style_line_width(wave, 2, 0);
    lv_obj_set_style_line_rounded(wave, true, 0);
}

// ── PPG wave icon (nhỏ, dùng trong card MAX30102) ───────────────
static lv_point_t s_ppg_pts[8];
static void draw_ppg_wave(lv_obj_t *parent, int16_t ox, int16_t oy)
{
    const int16_t dx[] = {0, 5, 8, 12, 15, 19, 24, 30};
    const int16_t dy[] = {0, 0, -8, 0, 6, 0, 0, 0};
    for (int i = 0; i < 8; i++)
    {
        s_ppg_pts[i].x = ox + dx[i];
        s_ppg_pts[i].y = oy + dy[i];
    }
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, s_ppg_pts, 8);
    lv_obj_set_style_line_color(line, lv_color_hex(C_PPG_WAVE), 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_opa(line, LV_OPA_50, 0);
}

// ── ECG wave icon (mini, dùng trong card AD8232) ─────────────────
static lv_point_t s_ecg_pts[10];
static void draw_ecg_wave(lv_obj_t *parent, int16_t ox, int16_t oy)
{
    const int16_t dx[] = {0, 6, 9, 12, 15, 18, 21, 25, 30, 36};
    const int16_t dy[] = {0, 0, -4, 8, -8, 4, 0, 0, 0, 0};
    for (int i = 0; i < 10; i++)
    {
        s_ecg_pts[i].x = ox + dx[i];
        s_ecg_pts[i].y = oy + dy[i];
    }
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, s_ecg_pts, 10);
    lv_obj_set_style_line_color(line, lv_color_hex(C_ECG_WAVE), 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_opa(line, LV_OPA_60, 0);
}

// ── Sensor card (156×60) ────────────────────────────────────────
static const char *S_TAG[5] = {"ECG", "PPG", "TEMP IR", "WiFi", "TOF DIST"};
static const char *S_NAME[5] = {"AD8232", "MAX30102", "MLX90614", "Network", "VL53L0X"};
static const char *S_DETAIL[5] = {"ECG Signal · 250Hz",
                                  "HR · SpO2",
                                  "Non-contact Temp",
                                  "802.11 b/g/n",
                                  "Proximity · Distance"};

static void build_sensor_card(int idx, int16_t cx, int16_t cy,
                              int16_t cw, int16_t ch)
{
    // Card container
    lv_obj_t *card = lv_obj_create(g_boot_scr);
    lv_obj_set_pos(card, cx, cy);
    lv_obj_set_size(card, cw, ch);
    lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(C_CARD_BDR), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    // Accent bar trái (ECG card và card đầu)
    if (idx == BOOT_SENSOR_AD8232)
    {
        mk_rect(card, 0, 0, 2, ch, C_CARD_ACCENT, 0);
    }

    // Tag (type label, góc trên trái)
    mk_label(card, S_TAG[idx], &lv_font_montserrat_10, C_TAG, 5, 3);

    // Sensor name — font 12, đậm
    mk_label(card, S_NAME[idx], &lv_font_montserrat_12, C_NAME, 5, 14);

    // Detail text — font 10
    mk_label(card, S_DETAIL[idx], &lv_font_montserrat_10, C_DETAIL, 5, 27);

    // WiFi: thêm dòng IP động
    if (idx == BOOT_SENSOR_WIFI)
    {
        g_lbl_wifi_ip = lv_label_create(card);
        lv_label_set_text(g_lbl_wifi_ip, "---");
        lv_obj_set_style_text_font(g_lbl_wifi_ip, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(g_lbl_wifi_ip, lv_color_hex(0x4A8090), 0);
        lv_obj_set_pos(g_lbl_wifi_ip, 5, 38);
    }

    // Waveform mini icons
    if (idx == BOOT_SENSOR_AD8232)
    {
        draw_ecg_wave(card, 4, ch - 10);
    }
    else if (idx == BOOT_SENSOR_MAX30102)
    {
        draw_ppg_wave(card, 4, ch - 10);
    }

    // Status badge — 48×16 px (đủ rộng cho "RETRY")
    int16_t bx = cw - 50;
    int16_t by = ch - 18;
    g_sensor_badge[idx] = mk_rect(card, bx, by, 46, 14, C_WAIT_BG, 3);
    g_sensor_status[idx] = lv_label_create(card);
    lv_label_set_text(g_sensor_status[idx], "WAIT");
    lv_obj_set_style_text_font(g_sensor_status[idx], &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(g_sensor_status[idx], lv_color_hex(C_WAIT_FG), 0);
    lv_obj_align_to(g_sensor_status[idx], g_sensor_badge[idx], LV_ALIGN_CENTER, 0, 0);
}

// ── TOF full-width bar ───────────────────────────────────────────
static void build_tof_bar(void)
{
    lv_obj_t *bar = lv_obj_create(g_boot_scr);
    lv_obj_set_pos(bar, CPAD, TOF_Y);
    lv_obj_set_size(bar, SCR_W - CPAD * 2, TOF_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(C_CARD_BDR), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 4, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    // Nội dung: tag + name + badge, căn theo chiều ngang
    mk_label(bar, "TOF DIST", &lv_font_montserrat_10, C_TAG, 5, 4);
    mk_label(bar, "VL53L0X", &lv_font_montserrat_12, C_NAME, 5, 15);
    mk_label(bar, "Proximity · Distance", &lv_font_montserrat_10, C_DETAIL, 80, 10);

    // Badge (dùng idx = BOOT_SENSOR_VL53L0X = 4)
    const int idx = BOOT_SENSOR_VL53L0X;
    int16_t bx = (SCR_W - CPAD * 2) - 50;
    int16_t by = (TOF_H - 14) / 2;
    g_sensor_badge[idx] = mk_rect(bar, bx, by, 46, 14, C_WAIT_BG, 3);
    g_sensor_status[idx] = lv_label_create(bar);
    lv_label_set_text(g_sensor_status[idx], "WAIT");
    lv_obj_set_style_text_font(g_sensor_status[idx], &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(g_sensor_status[idx], lv_color_hex(C_WAIT_FG), 0);
    lv_obj_align_to(g_sensor_status[idx], g_sensor_badge[idx], LV_ALIGN_CENTER, 0, 0);
}

// ── PUBLIC: build_boot() ─────────────────────────────────────────
void build_boot(void)
{
    // Màn hình chính
    g_boot_scr = lv_obj_create(NULL);
    lv_obj_set_size(g_boot_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(g_boot_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(g_boot_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_boot_scr, 0, 0);
    lv_obj_set_style_pad_all(g_boot_scr, 0, 0);
    lv_obj_clear_flag(g_boot_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_boot_scr, LV_SCROLLBAR_MODE_OFF);

    // Lưới nền (ECG paper, rất mờ)
    for (int y = 20; y < SCR_H; y += 20)
        mk_rect(g_boot_scr, 0, y, SCR_W, 1, C_GRID, 0);
    for (int x = 20; x < SCR_W; x += 20)
        mk_rect(g_boot_scr, x, 0, 1, SCR_H, C_GRID, 0);

    // ── HEADER ──────────────────────────────────────────────────
    lv_obj_t *hdr = mk_rect(g_boot_scr, 0, 0, SCR_W, HDR_H, C_HDR, 0);
    mk_rect(g_boot_scr, 0, HDR_H - 1, SCR_W, 1, C_HDR_LINE, 0);

    // Logo y tế (tâm ở cx=26, cy=21)
    draw_medical_logo(hdr, 26, 21);

    // Đường kẻ dọc phân cách logo / text
    mk_rect(hdr, LOGO_W, 7, 1, HDR_H - 14, 0x1C3535, 0);

    // Tên thiết bị
    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "BIOMONITOR");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(C_TITLE), 0);
    lv_obj_set_style_text_letter_space(lbl_title, 2, 0);
    lv_obj_set_pos(lbl_title, LOGO_W + 6, 3);

    // Subtitle
    lv_obj_t *lbl_sub = lv_label_create(hdr);
    lv_label_set_text(lbl_sub, "ECG \xc2\xb7 SpO2 \xc2\xb7 TEMP \xc2\xb7 IR");
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(C_SUBTITLE), 0);
    lv_obj_set_pos(lbl_sub, LOGO_W + 7, 24);

    // Version (góc phải header)
    lv_obj_t *lbl_ver = lv_label_create(hdr);
    lv_label_set_text(lbl_ver, "v2.0");
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(0x1A3040), 0);
    lv_obj_align(lbl_ver, LV_ALIGN_RIGHT_MID, -5, 0);

    // ── 4 SENSOR CARDS (2×2) ────────────────────────────────────
    build_sensor_card(BOOT_SENSOR_AD8232, CPAD, ROW1_Y, CW, CH_AB);
    build_sensor_card(BOOT_SENSOR_MAX30102, CPAD + CW + CGAP, ROW1_Y, CW, CH_AB);
    build_sensor_card(BOOT_SENSOR_MLX90614, CPAD, ROW2_Y, CW, CH_AB);
    build_sensor_card(BOOT_SENSOR_WIFI, CPAD + CW + CGAP, ROW2_Y, CW, CH_AB);

    // ── TOF FULL-WIDTH BAR ───────────────────────────────────────
    build_tof_bar();

    // ── PROGRESS BAR ────────────────────────────────────────────
    mk_rect(g_boot_scr, CPAD, PROG_Y, PROG_W, PROG_H, C_PROG_TRACK, 3); // track
    g_progress_fill = mk_rect(g_boot_scr, CPAD, PROG_Y, 0, PROG_H, C_PROG_FILL, 3);

    // ── INFO BAR (Device ID + Heap) ──────────────────────────────
    lv_obj_t *info = lv_obj_create(g_boot_scr);
    lv_obj_set_pos(info, CPAD, INFO_Y);
    lv_obj_set_size(info, SCR_W - CPAD * 2, INFO_H);
    lv_obj_set_style_bg_color(info, lv_color_hex(0x080F0F), 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(info, lv_color_hex(C_CARD_BDR), 0);
    lv_obj_set_style_border_width(info, 1, 0);
    lv_obj_set_style_radius(info, 3, 0);
    lv_obj_set_style_pad_all(info, 0, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(info, LV_SCROLLBAR_MODE_OFF);

    // Device ID
    char dev_buf[20];
    snprintf(dev_buf, sizeof(dev_buf), "ESP32_%06X",
             (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));
    mk_label(info, dev_buf, &lv_font_montserrat_10, 0x2A4050, 5, 3);

    // Status text (phần giữa)
    g_lbl_status = lv_label_create(info);
    lv_label_set_text(g_lbl_status, "Starting up...");
    lv_obj_set_style_text_font(g_lbl_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(g_lbl_status, lv_color_hex(C_DIM), 0);
    lv_obj_align(g_lbl_status, LV_ALIGN_CENTER, 0, 0);

    // Heap (bên phải)
    g_lbl_heap = lv_label_create(info);
    char heap_buf[20];
    snprintf(heap_buf, sizeof(heap_buf), "Heap:%dK", ESP.getFreeHeap() / 1024);
    lv_label_set_text(g_lbl_heap, heap_buf);
    lv_obj_set_style_text_font(g_lbl_heap, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(g_lbl_heap, lv_color_hex(0x2E5060), 0);
    lv_obj_align(g_lbl_heap, LV_ALIGN_RIGHT_MID, -5, 0);

    lv_scr_load(g_boot_scr);
}

// ── PUBLIC: set sensor status ────────────────────────────────────
void boot_screen_set_sensor_status(uint8_t idx, BootSensorStatus status)
{
    if (idx >= BOOT_SENSOR_COUNT)
        return;
    if (!g_sensor_badge[idx] || !g_sensor_status[idx])
        return;

    const char *txt;
    uint32_t bg, fg;

    switch (status)
    {
    case BOOT_SENSOR_OK:
        txt = "OK";
        bg = C_OK_BG;
        fg = C_OK_FG;
        break;
    case BOOT_SENSOR_RETRY:
        txt = "RETRY";
        bg = C_RETRY_BG;
        fg = C_RETRY_FG;
        break;
    case BOOT_SENSOR_FAIL:
        txt = "FAIL";
        bg = C_FAIL_BG;
        fg = C_FAIL_FG;
        break;
    case BOOT_SENSOR_OFF:
        txt = "OFF";
        bg = C_WAIT_BG;
        fg = 0x1E3040;
        break;
    default:
        txt = "...";
        bg = C_WAIT_BG;
        fg = C_DIM;
        break;
    }

    lv_obj_set_style_bg_color(g_sensor_badge[idx], lv_color_hex(bg), 0);
    lv_label_set_text(g_sensor_status[idx], txt);
    lv_obj_set_style_text_color(g_sensor_status[idx], lv_color_hex(fg), 0);
    lv_obj_align_to(g_sensor_status[idx], g_sensor_badge[idx], LV_ALIGN_CENTER, 0, 0);
}

// ── PUBLIC: set progress 0-100 ───────────────────────────────────
void boot_screen_set_progress(uint8_t pct)
{
    if (!g_progress_fill)
        return;
    if (pct > 100)
        pct = 100;
    lv_obj_set_width(g_progress_fill, (int16_t)((pct * PROG_W) / 100));
}

// ── PUBLIC: set status text ──────────────────────────────────────
void boot_screen_set_status_text(const char *text)
{
    if (g_lbl_status && text)
        lv_label_set_text(g_lbl_status, text);
}

// ── PUBLIC: set WiFi IP detail ───────────────────────────────────
void boot_screen_set_wifi_detail(const char *ssid, const char *ip)
{
    if (!g_lbl_wifi_ip)
        return;
    char buf[24];
    if (ip && ip[0] != '\0')
    {
        snprintf(buf, sizeof(buf), "%s", ip);
    }
    else if (ssid && ssid[0] != '\0')
    {
        snprintf(buf, sizeof(buf), "Connecting...");
    }
    else
    {
        snprintf(buf, sizeof(buf), "No network");
    }
    lv_label_set_text(g_lbl_wifi_ip, buf);
}

// ── PUBLIC: refresh heap ─────────────────────────────────────────
void boot_screen_refresh_heap(void)
{
    if (!g_lbl_heap)
        return;
    char buf[20];
    snprintf(buf, sizeof(buf), "Heap:%dK", ESP.getFreeHeap() / 1024);
    lv_label_set_text(g_lbl_heap, buf);
}