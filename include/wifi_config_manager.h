#ifndef WIFI_CONFIG_MANAGER_H
#define WIFI_CONFIG_MANAGER_H

#include <Arduino.h>

class WebServer;
class Preferences;

class WifiConfigManager
{
public:
    WifiConfigManager();
    ~WifiConfigManager();

    void begin();
    void update();

    bool isApMode() const;
    bool isConnected() const;
    String modeLabel() const;
    String ipAddress() const;
    String instructionText() const;
    String connectedSsid() const;
    String apSsid() const;
    bool mqttUseCustom() const;
    String mqttHost() const;
    uint16_t mqttPort() const;
    String mqttUser() const;
    String mqttPass() const;
    int signalRssi() const;
    bool isStaReconnectInProgress() const;
    String staReconnectSsid() const;
    uint32_t staReconnectElapsedMs() const;
    uint32_t staReconnectTimeoutMs() const;
    bool hasRecentStaReconnectTimeout(uint32_t windowMs = 8000) const;
    void forceStartAp();
    void exitConfigMode();
    bool consumeWebSaveSuccess(String &ssid);
    bool connectAndSaveFromLcd(const String &ssid, const String &pass);

private:
    WebServer *server_;
    Preferences *preferences_;
    bool serverStarted_;

    bool apMode_;
    bool webSaveSuccessPending_;
    String apSsid_;
    String lastWebSavedSsid_;
    String staSsid_;
    String staPass_;
    String email_;
    bool mqttUseCustom_;
    String mqttHost_;
    uint16_t mqttPort_;
    String mqttUser_;
    String mqttPass_;
    bool staReconnectInProgress_;
    uint32_t staReconnectStartMs_;
    uint32_t staReconnectTimeoutMs_;
    bool staReconnectTimedOut_;
    uint32_t staReconnectTimeoutAtMs_;

    void loadConfig();
    void saveConfig();
    void processStaReconnect();

    bool connectToSta(const String &ssid, const String &pass, uint32_t timeoutMs);
    void startAccessPoint();
    void startServer();

    void handleRoot();
    void handleScan();
    void handleGetConfig();
    void handleSave();
};

#endif