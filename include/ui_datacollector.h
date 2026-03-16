#ifndef UI_DATACOLLECTOR_H
#define UI_DATACOLLECTOR_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // ==========================================
    // SCREENS
    // ==========================================
    extern lv_obj_t *ui_MonitorScreen;

    // ==========================================
    // MONITOR SCREEN COMPONENTS
    // ==========================================
    // Header
    extern lv_obj_t *ui_HeaderPanel;
    extern lv_obj_t *ui_TitleLabel;
    extern lv_obj_t *ui_WiFiIcon;

    // Info panels
    extern lv_obj_t *ui_IDPanel;
    extern lv_obj_t *ui_IDLabel;
    extern lv_obj_t *ui_IDValue;

    extern lv_obj_t *ui_ProgressPanel;
    extern lv_obj_t *ui_ProgressLabel;
    extern lv_obj_t *ui_ProgressValue;

    // Data panels
    extern lv_obj_t *ui_DistancePanel;
    extern lv_obj_t *ui_DistanceLabel;
    extern lv_obj_t *ui_DistanceValue;
    extern lv_obj_t *ui_DistanceUnit;

    extern lv_obj_t *ui_TempPanel;
    extern lv_obj_t *ui_TempLabel;
    extern lv_obj_t *ui_TempValue;
    extern lv_obj_t *ui_TempUnit;

    extern lv_obj_t *ui_AmbientPanel;
    extern lv_obj_t *ui_AmbientLabel;
    extern lv_obj_t *ui_AmbientValue;
    extern lv_obj_t *ui_AmbientUnit;

    // Footer
    extern lv_obj_t *ui_FooterPanel;
    extern lv_obj_t *ui_InstructionLabel1;
    extern lv_obj_t *ui_InstructionLabel2;
    extern lv_obj_t *ui_InstructionLabel3;

    // Popup
    extern lv_obj_t *ui_PopupPanel;
    extern lv_obj_t *ui_PopupTitle;
    extern lv_obj_t *ui_PopupMessage;
    extern lv_obj_t *ui_PopupProgressBar;

    // ==========================================
    // PUBLIC FUNCTIONS
    // ==========================================
    /**
     * @brief Initialize all UI screens and components
     */
    void ui_datacollector_init(void);

    /**
     * @brief Update real-time sensor data on Monitor screen
     * @param distance Distance in mm
     * @param temperature Object temperature in Celsius
     * @param ambient Ambient temperature in Celsius
     */
    void ui_datacollector_update_sensors(float distance, float temperature, float ambient);

    /**
     * @brief Update ID value on Monitor screen
     * @param id Current person ID
     */
    void ui_datacollector_update_id(int id);

    /**
     * @brief Update progress counter
     * @param current Current sample count
     * @param total Total samples needed (default 5)
     */
    void ui_datacollector_update_progress(int current, int total);

    /**
     * @brief Update WiFi status icon
     * @param connected true if connected, false otherwise
     */
    void ui_datacollector_update_wifi(bool connected);

    /**
     * @brief Show popup with message
     * @param title Popup title
     * @param message Popup message
     * @param color Background color (e.g., lv_palette_main(LV_PALETTE_GREEN))
     * @param show_progress Show progress bar
     */
    void ui_datacollector_show_popup(const char *title, const char *message,
                                     lv_color_t color, bool show_progress);

    /**
     * @brief Hide popup
     */
    void ui_datacollector_hide_popup(void);

    /**
     * @brief Update popup progress bar (0-100)
     */
    void ui_datacollector_update_popup_progress(int percentage);

    /**
     * @brief Switch to Monitor screen
     */
    void ui_datacollector_load_monitor(void);

    // ==========================================
    // EVENT CALLBACKS (to be implemented in main.cpp)
    // ==========================================
    /**
     * @brief Called when ENTER button pressed (take measurement)
     */
    void ui_event_take_measurement(lv_event_t *e);

    /**
     * @brief Called when ID minus button pressed
     */
    void ui_event_id_minus(lv_event_t *e);

    /**
     * @brief Called when ID plus button pressed
     */
    void ui_event_id_plus(lv_event_t *e);

    /**
     * @brief Called when sample reset button pressed
     */
    void ui_event_sample_reset(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif // UI_DATACOLLECTOR_H
