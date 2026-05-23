#include "lcd_ui_presenter.h"

#include "ui.h"
#include "wifi_config_manager.h"

void refresh_config_ui(WifiConfigManager &wifiConfigManager)
{
    if (wifiConfigManager.isStaReconnectInProgress())
    {
        const uint32_t elapsedMs = wifiConfigManager.staReconnectElapsedMs();
        const uint32_t timeoutMs = wifiConfigManager.staReconnectTimeoutMs();
        uint32_t remainingMs = 0;
        if (timeoutMs > elapsedMs)
        {
            remainingMs = timeoutMs - elapsedMs;
        }

        const uint32_t remainingSec = (remainingMs + 999) / 1000;

        ui_set_config_ip("0.0.0.0");
        ui_set_config_status("RECONNECTING SAVED WIFI...", 0xFFB300);

        String instruction = "Trying ";
        instruction += wifiConfigManager.staReconnectSsid();
        instruction += " (timeout in ";
        instruction += String(remainingSec);
        instruction += "s)";
        ui_set_config_instruction(instruction.c_str());
        return;
    }

    if (wifiConfigManager.hasRecentStaReconnectTimeout())
    {
        ui_set_config_ip("0.0.0.0");
        ui_set_config_status("RECONNECT TIMEOUT", 0xFF5252);
        ui_set_config_instruction("Saved WiFi unavailable. Open Config to retry.");
        return;
    }

    String ip = wifiConfigManager.ipAddress();
    ui_set_config_ip(ip.c_str());

    String status = wifiConfigManager.modeLabel();
    uint32_t color = 0xFF5252;

    if (wifiConfigManager.isApMode())
    {
        status += " | ";
        status += wifiConfigManager.apSsid();
        color = 0xFFB300;
    }
    else if (wifiConfigManager.isConnected())
    {
        String ssid = wifiConfigManager.connectedSsid();
        if (!ssid.isEmpty())
        {
            status += " | ";
            status += ssid;
        }
        color = 0x00E676;
    }

    ui_set_config_status(status.c_str(), color);

    String instruction = wifiConfigManager.instructionText();
    ui_set_config_instruction(instruction.c_str());
}

void refresh_wifi_header_ui(WifiConfigManager &wifiConfigManager)
{
    String ssid = "OFF";
    String level = "0/4";
    uint32_t color = 0xFF5252;

    if (wifiConfigManager.isStaReconnectInProgress())
    {
        ssid = wifiConfigManager.staReconnectSsid();
        if (ssid.isEmpty())
        {
            ssid = "RECONNECTING";
        }
        level = "...";
        color = 0xFFB300;
        ui_set_header_wifi(level.c_str(), ssid.c_str(), color);
        return;
    }

    if (wifiConfigManager.hasRecentStaReconnectTimeout())
    {
        ssid = "WIFI TIMEOUT";
        level = "!";
        color = 0xFF5252;
        ui_set_header_wifi(level.c_str(), ssid.c_str(), color);
        return;
    }

    if (wifiConfigManager.isApMode())
    {
        ssid = wifiConfigManager.apSsid();
        level = "AP";
        color = 0xFFB300;
    }
    else if (wifiConfigManager.isConnected())
    {
        ssid = wifiConfigManager.connectedSsid();
        int rssi = wifiConfigManager.signalRssi();

        int bars = 0;
        if (rssi >= -55)
            bars = 4;
        else if (rssi >= -67)
            bars = 3;
        else if (rssi >= -75)
            bars = 2;
        else if (rssi >= -85)
            bars = 1;

        char levelText[8];
        snprintf(levelText, sizeof(levelText), "%d/4", bars);
        level = levelText;

        if (bars >= 3)
            color = 0x00E676;
        else if (bars >= 2)
            color = 0xFFB300;
        else
            color = 0xFF7043;
    }

    ui_set_header_wifi(level.c_str(), ssid.c_str(), color);
}

void notify_max_state(MaxRuntimeState state, bool monitorMode, bool maxReady, bool mlxReady)
{
    if (monitorMode)
    {
        if (!maxReady && !mlxReady)
        {
            ui_set_sensor_status("NO MAX30102 | NO MLX", 0xFF5252);
        }
        else if (!maxReady && mlxReady)
        {
            ui_set_sensor_status("NO MAX30102 | MLX OK", 0xFFB300);
        }
        else if (state == MaxRuntimeState::WaitingFinger && mlxReady)
        {
            ui_set_sensor_status("MLX OK | PLACE FINGER", 0xFFB300);
        }
        else if (state == MaxRuntimeState::LiveData && mlxReady)
        {
            ui_set_sensor_status("MAX30102 + MLX OK", 0x00E676);
        }
        else if (state == MaxRuntimeState::LiveData)
        {
            ui_set_sensor_status("MAX30102 OK | NO MLX", 0xFFB300);
        }
        else
        {
            ui_set_sensor_status("PLACE FINGER ON SENSOR", 0xFFB300);
        }
        return;
    }

    if (state == MaxRuntimeState::NoSensor)
    {
        ui_set_sensor_status("NO MAX30102 - CHECK WIRING", 0xFF5252);
    }
    else if (state == MaxRuntimeState::WaitingFinger)
    {
        ui_set_sensor_status("PLACE FINGER ON SENSOR", 0xFFB300);
    }
    else
    {
        ui_set_sensor_status("MAX30102 LIVE", 0x00E676);
    }
}

void notify_ecg_state(EcgRuntimeState state)
{
    if (state == EcgRuntimeState::NoSensor)
    {
        ui_set_sensor_status("NO SENSOR", 0xFF5252);
    }
    else if (state == EcgRuntimeState::LeadsOff)
    {
        ui_set_sensor_status("LEADS OFF", 0xFFB300);
    }
    else
    {
        ui_set_sensor_status("LIVE", 0xFF1744);
    }
}
