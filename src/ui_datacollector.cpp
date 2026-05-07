#include "ui_datacollector.h"
#include <Arduino.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // ==========================================
    // SCREEN OBJECTS
    // ==========================================
    lv_obj_t *ui_MonitorScreen;

    // ==========================================
    // MONITOR SCREEN COMPONENTS
    // ==========================================
    lv_obj_t *ui_HeaderPanel;
    lv_obj_t *ui_TitleLabel;
    lv_obj_t *ui_WiFiIcon;
    lv_obj_t *ui_WiFiSignal;
    lv_obj_t *ui_WiFiSsid;

    lv_obj_t *ui_IDPanel;
    lv_obj_t *ui_IDLabel;
    lv_obj_t *ui_IDValue;

    lv_obj_t *ui_ProgressPanel;
    lv_obj_t *ui_ProgressLabel;
    lv_obj_t *ui_ProgressValue;

    lv_obj_t *ui_DistancePanel;
    lv_obj_t *ui_DistanceLabel;
    lv_obj_t *ui_DistanceValue;
    lv_obj_t *ui_DistanceUnit;

    lv_obj_t *ui_TempPanel;
    lv_obj_t *ui_TempLabel;
    lv_obj_t *ui_TempValue;
    lv_obj_t *ui_TempUnit;

    lv_obj_t *ui_AmbientPanel;
    lv_obj_t *ui_AmbientLabel;
    lv_obj_t *ui_AmbientValue;
    lv_obj_t *ui_AmbientUnit;

    lv_obj_t *ui_FooterPanel;
    lv_obj_t *ui_InstructionLabel1;
    lv_obj_t *ui_InstructionLabel2;
    lv_obj_t *ui_InstructionLabel3;

    lv_obj_t *ui_PopupPanel;
    lv_obj_t *ui_PopupTitle;
    lv_obj_t *ui_PopupMessage;
    lv_obj_t *ui_PopupProgressBar;

    // ==========================================
    // HELPER FUNCTIONS
    // ==========================================
    static lv_obj_t *create_panel(lv_obj_t *parent, int x, int y, int w, int h,
                                  lv_color_t bg_color, int radius, bool has_border)
    {
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_style_bg_color(panel, bg_color, 0);
        lv_obj_set_style_radius(panel, radius, 0);
        lv_obj_set_style_border_width(panel, has_border ? 2 : 0, 0);
        lv_obj_set_style_pad_all(panel, 0, 0); // Remove padding to prevent layout issues
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        return panel;
    }

    static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y,
                                  lv_color_t color, const lv_font_t *font,
                                  lv_align_t align = LV_ALIGN_TOP_LEFT)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, color, 0);
        lv_obj_set_style_text_font(label, font, 0);
        if (align == LV_ALIGN_TOP_LEFT)
        {
            lv_obj_set_pos(label, x, y);
        }
        else
        {
            lv_obj_align(label, align, x, y);
        }
        return label;
    }

    // ==========================================
    // CREATE MONITOR SCREEN
    // ==========================================
    static void ui_create_monitor_screen(void)
    {
        ui_MonitorScreen = lv_obj_create(NULL);
        lv_obj_clear_flag(ui_MonitorScreen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(ui_MonitorScreen, lv_color_hex(0x000000), 0);

        // ===== HEADER =====
        ui_HeaderPanel = create_panel(ui_MonitorScreen, 0, 0, 320, 25,
                                      lv_color_hex(0x001F), 0, false);

        ui_TitleLabel = create_label(ui_HeaderPanel, "DATA COLLECTOR", 5, 4,
                                     lv_color_hex(0xFFFFFF), &lv_font_montserrat_14);

        ui_WiFiIcon = create_label(ui_HeaderPanel, LV_SYMBOL_WIFI, 0, 0,
                                   lv_color_hex(0x00FF00), &lv_font_montserrat_14,
                                   LV_ALIGN_TOP_RIGHT);
        lv_obj_align(ui_WiFiIcon, LV_ALIGN_TOP_RIGHT, -5, 4);

        ui_WiFiSignal = create_label(ui_HeaderPanel, "0/4", 0, 0,
                                     lv_color_hex(0x00FF00), &lv_font_montserrat_10,
                                     LV_ALIGN_TOP_RIGHT);
        lv_obj_set_width(ui_WiFiSignal, 24);
        lv_obj_set_style_text_align(ui_WiFiSignal, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(ui_WiFiSignal, LV_ALIGN_TOP_RIGHT, -28, 6);

        ui_WiFiSsid = create_label(ui_HeaderPanel, "OFFLINE", 0, 0,
                                   lv_color_hex(0x00FF00), &lv_font_montserrat_10,
                                   LV_ALIGN_TOP_RIGHT);
        lv_obj_set_width(ui_WiFiSsid, 60);
        lv_label_set_long_mode(ui_WiFiSsid, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(ui_WiFiSsid, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(ui_WiFiSsid, LV_ALIGN_TOP_RIGHT, -54, 6);

        // ===== INFO PANELS =====
        // ID Panel
        ui_IDPanel = create_panel(ui_MonitorScreen, 5, 27, 155, 23,
                                  lv_color_hex(0x0A0A), 3, true);
        lv_obj_set_style_border_color(ui_IDPanel, lv_color_hex(0x4208), 0);

        ui_IDLabel = create_label(ui_IDPanel, "ID:", 4, 3,
                                  lv_color_hex(0x00FFFF), &lv_font_montserrat_12);

        ui_IDValue = create_label(ui_IDPanel, "1", 35, 2,
                                  lv_color_hex(0xFFFFFF), &lv_font_montserrat_16);

        // Progress Panel
        ui_ProgressPanel = create_panel(ui_MonitorScreen, 165, 27, 150, 23,
                                        lv_color_hex(0x0A00), 3, true);
        lv_obj_set_style_border_color(ui_ProgressPanel, lv_color_hex(0x8800), 0);

        ui_ProgressLabel = create_label(ui_ProgressPanel, "MAU:", 4, 3,
                                        lv_color_hex(0xFFFF00), &lv_font_montserrat_12);

        ui_ProgressValue = create_label(ui_ProgressPanel, "0/5", 50, 2,
                                        lv_color_hex(0xFFFFFF), &lv_font_montserrat_16);

        // ===== DATA PANELS (3 columns) =====
        // Distance Panel - Left
        ui_DistancePanel = create_panel(ui_MonitorScreen, 5, 52, 100, 108,
                                        lv_color_hex(0x001A), 5, true);
        lv_obj_set_style_border_color(ui_DistancePanel, lv_color_hex(0x001F), 0);
        lv_obj_set_style_border_width(ui_DistancePanel, 2, 0);

        ui_DistanceLabel = create_label(ui_DistancePanel, "KHOANG CACH", 0, 5,
                                        lv_color_hex(0x07FF), &lv_font_montserrat_10,
                                        LV_ALIGN_TOP_MID);

        ui_DistanceValue = create_label(ui_DistancePanel, "0", 0, 35,
                                        lv_color_hex(0x07FF), &lv_font_montserrat_28,
                                        LV_ALIGN_TOP_MID);

        ui_DistanceUnit = create_label(ui_DistancePanel, "mm ", 0, -8,
                                       lv_color_hex(0x6B6B), &lv_font_montserrat_14,
                                       LV_ALIGN_BOTTOM_MID);

        // Temperature Object Panel - Center
        ui_TempPanel = create_panel(ui_MonitorScreen, 110, 52, 100, 108,
                                    lv_color_hex(0x1A00), 5, true);
        lv_obj_set_style_border_color(ui_TempPanel, lv_color_hex(0x8800), 0);
        lv_obj_set_style_border_width(ui_TempPanel, 2, 0);

        ui_TempLabel = create_label(ui_TempPanel, "NHIET DO DA", 0, 5,
                                    lv_color_hex(0xFDA0), &lv_font_montserrat_10,
                                    LV_ALIGN_TOP_MID);

        ui_TempValue = create_label(ui_TempPanel, "0.0", 0, 35,
                                    lv_color_hex(0xFDA0), &lv_font_montserrat_28,
                                    LV_ALIGN_TOP_MID);

        ui_TempUnit = create_label(ui_TempPanel, "C", 0, -8,
                                   lv_color_hex(0x6B6B), &lv_font_montserrat_14,
                                   LV_ALIGN_BOTTOM_MID);

        // Temperature Ambient Panel - Right
        ui_AmbientPanel = create_panel(ui_MonitorScreen, 215, 52, 100, 108,
                                       lv_color_hex(0x0A1A), 5, true);
        lv_obj_set_style_border_color(ui_AmbientPanel, lv_color_hex(0x0880), 0);
        lv_obj_set_style_border_width(ui_AmbientPanel, 2, 0);

        ui_AmbientLabel = create_label(ui_AmbientPanel, "M.TRUONG", 0, 5,
                                       lv_color_hex(0x07E0), &lv_font_montserrat_10,
                                       LV_ALIGN_TOP_MID);

        ui_AmbientValue = create_label(ui_AmbientPanel, "0.0", 0, 35,
                                       lv_color_hex(0x07E0), &lv_font_montserrat_28,
                                       LV_ALIGN_TOP_MID);

        ui_AmbientUnit = create_label(ui_AmbientPanel, "C", 0, -8,
                                      lv_color_hex(0x6B6B), &lv_font_montserrat_14,
                                      LV_ALIGN_BOTTOM_MID);

        // ===== FOOTER =====
        ui_FooterPanel = create_panel(ui_MonitorScreen, 0, 162, 320, 78,
                                      lv_color_hex(0x1A1A), 0, false);
        lv_obj_set_style_border_side(ui_FooterPanel, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(ui_FooterPanel, 1, 0);
        lv_obj_set_style_border_color(ui_FooterPanel, lv_color_hex(0x4208), 0);

        ui_InstructionLabel1 = create_label(ui_FooterPanel, "[ENTER] Lay mau & Gui MQTT", 5, 5,
                                            lv_color_hex(0xFFFF00), &lv_font_montserrat_12);

        ui_InstructionLabel2 = create_label(ui_FooterPanel, "[LEFT] ID-  [RIGHT] ID+  (Hold LEFT: Back)", 5, 23,
                                            lv_color_hex(0x9CF3), &lv_font_montserrat_10);

        ui_InstructionLabel3 = create_label(ui_FooterPanel, "[DOWN] Reset mau", 5, 38,
                                            lv_color_hex(0x9CF3), &lv_font_montserrat_10);

        lv_obj_t *ui_InfoLabel = create_label(ui_FooterPanel, "Khoang cach 20-30mm roi bam ENTER", 5, 55,
                                              lv_color_hex(0x6B6B), &lv_font_montserrat_10);

        // ===== POPUP (HIDDEN) - Professional Design =====
        ui_PopupPanel = lv_obj_create(ui_MonitorScreen);
        lv_obj_set_size(ui_PopupPanel, 280, 120);
        lv_obj_center(ui_PopupPanel);
        lv_obj_set_style_bg_color(ui_PopupPanel, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_radius(ui_PopupPanel, 10, 0);
        lv_obj_set_style_border_color(ui_PopupPanel, lv_color_hex(0x07FF), 0);
        lv_obj_set_style_border_width(ui_PopupPanel, 3, 0);
        lv_obj_set_style_shadow_width(ui_PopupPanel, 15, 0);
        lv_obj_set_style_shadow_color(ui_PopupPanel, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(ui_PopupPanel, LV_OPA_50, 0);
        lv_obj_set_style_pad_all(ui_PopupPanel, 0, 0); // Remove padding
        lv_obj_clear_flag(ui_PopupPanel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ui_PopupPanel, LV_OBJ_FLAG_HIDDEN);

        // Icon area
        lv_obj_t *icon_area = create_panel(ui_PopupPanel, 10, 10, 40, 40,
                                           lv_color_hex(0x001F), 20, false);
        lv_obj_t *popup_icon = create_label(icon_area, LV_SYMBOL_UPLOAD, 0, 0,
                                            lv_color_hex(0x07FF), &lv_font_montserrat_24,
                                            LV_ALIGN_CENTER);

        ui_PopupTitle = create_label(ui_PopupPanel, "TITLE", 60, 8,
                                     lv_color_hex(0xFFFFFF), &lv_font_montserrat_16);

        ui_PopupMessage = create_label(ui_PopupPanel, "Message", 60, 30,
                                       lv_color_hex(0xADB5BD), &lv_font_montserrat_12);

        // Progress bar with rounded corners
        ui_PopupProgressBar = lv_bar_create(ui_PopupPanel);
        lv_obj_set_size(ui_PopupProgressBar, 260, 18);
        lv_obj_align(ui_PopupProgressBar, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_color(ui_PopupProgressBar, lv_color_hex(0x2D3436), 0);
        lv_obj_set_style_bg_color(ui_PopupProgressBar, lv_color_hex(0x00D2FF), LV_PART_INDICATOR);
        lv_obj_set_style_radius(ui_PopupProgressBar, 9, 0);
        lv_obj_set_style_radius(ui_PopupProgressBar, 9, LV_PART_INDICATOR);
        lv_bar_set_value(ui_PopupProgressBar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(ui_PopupProgressBar, LV_OBJ_FLAG_HIDDEN);
    }

    // ==========================================
    // MAIN INIT FUNCTION
    // ==========================================
    void ui_datacollector_init(void)
    {
        ui_create_monitor_screen();

        // Load Monitor screen by default
        lv_disp_load_scr(ui_MonitorScreen);
    }

    // ==========================================
    // UPDATE FUNCTIONS
    // ==========================================
    void ui_datacollector_update_sensors(float distance, float temperature, float ambient)
    {
        static char dist_buf[16];
        static char temp_buf[16];
        static char amb_buf[16];

        if (distance >= 999)
        {
            lv_label_set_text(ui_DistanceValue, "---");
        }
        else
        {
            snprintf(dist_buf, sizeof(dist_buf), "%.0f", distance);
            lv_label_set_text(ui_DistanceValue, dist_buf);
        }

        snprintf(temp_buf, sizeof(temp_buf), "%.1f", temperature);
        lv_label_set_text(ui_TempValue, temp_buf);

        snprintf(amb_buf, sizeof(amb_buf), "%.1f", ambient);
        lv_label_set_text(ui_AmbientValue, amb_buf);
    }

    void ui_datacollector_update_id(int id)
    {
        static char buf[8];
        snprintf(buf, sizeof(buf), "%d", id);
        lv_label_set_text(ui_IDValue, buf);
    }

    void ui_datacollector_update_progress(int current, int total)
    {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d/%d", current, total);
        lv_label_set_text(ui_ProgressValue, buf);
    }

    void ui_datacollector_update_wifi(bool connected)
    {
        if (connected)
        {
            ui_datacollector_update_wifi_detail("1/4", "ONLINE", 0x00E676);
        }
        else
        {
            ui_datacollector_update_wifi_detail("0/4", "OFFLINE", 0xFF5252);
        }
    }

    void ui_datacollector_update_wifi_detail(const char *signalLevel, const char *ssid, uint32_t colorHex)
    {
        if (!ui_WiFiIcon || !ui_WiFiSignal || !ui_WiFiSsid)
        {
            return;
        }

        lv_obj_set_style_text_color(ui_WiFiIcon, lv_color_hex(colorHex), 0);
        lv_obj_set_style_text_color(ui_WiFiSignal, lv_color_hex(colorHex), 0);
        lv_obj_set_style_text_color(ui_WiFiSsid, lv_color_hex(colorHex), 0);
        lv_label_set_text(ui_WiFiSignal, signalLevel ? signalLevel : "0/4");
        lv_label_set_text(ui_WiFiSsid, ssid ? ssid : "OFFLINE");
    }

    void ui_datacollector_show_popup(const char *title, const char *message,
                                     lv_color_t color, bool show_progress)
    {
        // Professional appearance: Keep dark background, update border and title color
        lv_obj_set_style_border_color(ui_PopupPanel, color, 0);
        lv_obj_set_style_shadow_color(ui_PopupPanel, color, 0);
        lv_obj_set_style_text_color(ui_PopupTitle, color, 0);

        lv_label_set_text(ui_PopupTitle, title);
        lv_label_set_text(ui_PopupMessage, message);

        if (show_progress)
        {
            lv_obj_clear_flag(ui_PopupProgressBar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(ui_PopupProgressBar, 0, LV_ANIM_OFF);
        }
        else
        {
            lv_obj_add_flag(ui_PopupProgressBar, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_clear_flag(ui_PopupPanel, LV_OBJ_FLAG_HIDDEN);
    }

    void ui_datacollector_hide_popup(void)
    {
        if (!ui_PopupPanel)
        {
            return;
        }
        lv_obj_add_flag(ui_PopupPanel, LV_OBJ_FLAG_HIDDEN);
    }

    void ui_datacollector_update_popup_progress(int percentage)
    {
        if (!ui_PopupProgressBar)
        {
            return;
        }
        lv_bar_set_value(ui_PopupProgressBar, percentage, LV_ANIM_OFF);
    }

    void ui_datacollector_load_monitor(void)
    {
        lv_disp_load_scr(ui_MonitorScreen);
    }

    void ui_datacollector_cleanup(void)
    {
        if (ui_MonitorScreen)
        {
            // Screen is deleted by ui_switch_screen; only drop pointers here.
            ui_MonitorScreen = NULL;

            // Reset all object pointers
            ui_HeaderPanel = NULL;
            ui_TitleLabel = NULL;
            ui_WiFiIcon = NULL;
            ui_WiFiSignal = NULL;
            ui_WiFiSsid = NULL;
            ui_IDPanel = NULL;
            ui_IDLabel = NULL;
            ui_IDValue = NULL;
            ui_ProgressPanel = NULL;
            ui_ProgressLabel = NULL;
            ui_ProgressValue = NULL;
            ui_DistancePanel = NULL;
            ui_DistanceLabel = NULL;
            ui_DistanceValue = NULL;
            ui_DistanceUnit = NULL;
            ui_TempPanel = NULL;
            ui_TempLabel = NULL;
            ui_TempValue = NULL;
            ui_TempUnit = NULL;
            ui_AmbientPanel = NULL;
            ui_AmbientLabel = NULL;
            ui_AmbientValue = NULL;
            ui_AmbientUnit = NULL;
            ui_FooterPanel = NULL;
            ui_InstructionLabel1 = NULL;
            ui_InstructionLabel2 = NULL;
            ui_InstructionLabel3 = NULL;
            ui_PopupPanel = NULL;
            ui_PopupTitle = NULL;
            ui_PopupMessage = NULL;
            ui_PopupProgressBar = NULL;
        }
    }

#ifdef __cplusplus
}
#endif
