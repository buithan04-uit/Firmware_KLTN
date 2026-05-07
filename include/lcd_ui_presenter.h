#ifndef LCD_UI_PRESENTER_H
#define LCD_UI_PRESENTER_H

#include <Arduino.h>

class WifiConfigManager;

enum class MaxRuntimeState
{
    NoSensor,
    WaitingFinger,
    LiveData
};

enum class EcgRuntimeState
{
    NoSensor,
    LeadsOff,
    LiveData
};

void refresh_config_ui(WifiConfigManager &wifiConfigManager);
void refresh_wifi_header_ui(WifiConfigManager &wifiConfigManager);
void notify_max_state(MaxRuntimeState state, bool monitorMode, bool maxReady, bool mlxReady);
void notify_ecg_state(EcgRuntimeState state);

#endif
