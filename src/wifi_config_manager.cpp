#include "wifi_config_manager.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "web_interface.h"
#include "mqtt_defaults.h"

WifiConfigManager::WifiConfigManager()
    : server_(nullptr),
      preferences_(nullptr),
      serverStarted_(false),
      apMode_(false),
      webSaveSuccessPending_(false),
      mqttUseCustom_(false),
      mqttPort_(kDefaultMqttPort),
      staReconnectInProgress_(false),
      staReconnectStartMs_(0),
      staReconnectTimeoutMs_(0),
      staReconnectTimedOut_(false),
      staReconnectTimeoutAtMs_(0)
{
}

WifiConfigManager::~WifiConfigManager()
{
    if (server_)
    {
        delete server_;
        server_ = nullptr;
    }
    if (preferences_)
    {
        delete preferences_;
        preferences_ = nullptr;
    }
    serverStarted_ = false;
}

void WifiConfigManager::begin()
{
    if (!server_)
    {
        server_ = new WebServer(80);
    }
    if (!preferences_)
    {
        preferences_ = new Preferences();
    }

    loadConfig();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);

    bool connected = false;
    bool hasSavedSta = !staSsid_.isEmpty();
    if (hasSavedSta)
    {
        Serial.printf("[WIFI] Connecting to saved SSID: %s\n", staSsid_.c_str());
        connected = connectToSta(staSsid_, staPass_, 12000);
    }

    apMode_ = false;
    WiFi.mode(WIFI_STA);

    if (connected)
    {
        Serial.printf("[WIFI] STA connected. IP: %s\n", WiFi.localIP().toString().c_str());
    }
    else if (hasSavedSta)
    {
        Serial.println("[WIFI] Saved WiFi connect failed. Staying OFFLINE until Config mode.");
    }
    else
    {
        Serial.println("[WIFI] No saved WiFi yet. Staying OFFLINE until Config mode.");
    }

    // Only start web config server when user enters Config screen.
}

void WifiConfigManager::update()
{
    if (serverStarted_ && server_)
    {
        server_->handleClient();
    }

    processStaReconnect();
}

bool WifiConfigManager::isApMode() const
{
    return apMode_;
}

bool WifiConfigManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String WifiConfigManager::modeLabel() const
{
    if (apMode_)
    {
        return "AP MODE";
    }
    if (staReconnectInProgress_)
    {
        return "RECONNECTING WIFI";
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        return "STA CONNECTED";
    }
    if (hasRecentStaReconnectTimeout())
    {
        return "RECONNECT TIMEOUT";
    }
    return "WIFI OFFLINE";
}

String WifiConfigManager::ipAddress() const
{
    if (apMode_)
    {
        return WiFi.softAPIP().toString();
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String WifiConfigManager::instructionText() const
{
    if (apMode_)
    {
        String msg = "Join AP ";
        msg += apSsid_;
        msg += " (pass 12345678), open http://";
        msg += ipAddress();
        return msg;
    }

    if (staReconnectInProgress_)
    {
        String msg = "Reconnecting to ";
        msg += staSsid_;
        msg += "...";
        return msg;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        String msg = "Open browser at http://";
        msg += ipAddress();
        msg += " on the same WiFi";
        return msg;
    }

    if (hasRecentStaReconnectTimeout())
    {
        return "Reconnect timeout. Open Config to retry";
    }

    return "Open Config screen to start AP web setup";
}

String WifiConfigManager::connectedSsid() const
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.SSID();
    }
    return "";
}

String WifiConfigManager::apSsid() const
{
    return apSsid_;
}

bool WifiConfigManager::mqttUseCustom() const
{
    return mqttUseCustom_;
}

String WifiConfigManager::mqttHost() const
{
    if (!mqttUseCustom_ || mqttHost_.isEmpty())
    {
        return String(kDefaultMqttHost);
    }
    return mqttHost_;
}

uint16_t WifiConfigManager::mqttPort() const
{
    if (!mqttUseCustom_ || mqttPort_ == 0)
    {
        return kDefaultMqttPort;
    }
    return mqttPort_;
}

String WifiConfigManager::mqttUser() const
{
    if (!mqttUseCustom_ || mqttUser_.isEmpty())
    {
        return String(kDefaultMqttUser);
    }
    return mqttUser_;
}

String WifiConfigManager::mqttPass() const
{
    if (!mqttUseCustom_ || mqttPass_.isEmpty())
    {
        return String(kDefaultMqttPass);
    }
    return mqttPass_;
}

int WifiConfigManager::signalRssi() const
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.RSSI();
    }
    return -127;
}

bool WifiConfigManager::isStaReconnectInProgress() const
{
    return staReconnectInProgress_;
}

String WifiConfigManager::staReconnectSsid() const
{
    return staSsid_;
}

uint32_t WifiConfigManager::staReconnectElapsedMs() const
{
    if (!staReconnectInProgress_)
    {
        return 0;
    }
    return millis() - staReconnectStartMs_;
}

uint32_t WifiConfigManager::staReconnectTimeoutMs() const
{
    return staReconnectTimeoutMs_;
}

bool WifiConfigManager::hasRecentStaReconnectTimeout(uint32_t windowMs) const
{
    if (!staReconnectTimedOut_)
    {
        return false;
    }
    return (millis() - staReconnectTimeoutAtMs_) <= windowMs;
}

void WifiConfigManager::forceStartAp()
{
    staReconnectInProgress_ = false;
    staReconnectTimedOut_ = false;

    if (!serverStarted_)
    {
        startServer();
        serverStarted_ = true;
    }

    if (!apMode_)
    {
        startAccessPoint();
    }

    Serial.printf("[WIFI] Manual AP force. SSID: %s, IP: %s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
}

void WifiConfigManager::exitConfigMode()
{
    staReconnectInProgress_ = false;

    WiFi.softAPdisconnect(true);
    if (apMode_)
    {
        Serial.println("[WIFI] Config mode exit: AP stopped.");
    }
    apMode_ = false;

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WIFI] Config mode exit: keeping active WiFi %s\n", WiFi.SSID().c_str());
        return;
    }

    if (!staSsid_.isEmpty())
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(staSsid_.c_str(), staPass_.c_str());

        staReconnectInProgress_ = true;
        staReconnectStartMs_ = millis();
        staReconnectTimeoutMs_ = 10000;
        staReconnectTimedOut_ = false;

        Serial.printf("[WIFI] Config mode exit: reconnecting saved WiFi %s (async)\n", staSsid_.c_str());
    }
    else
    {
        WiFi.mode(WIFI_STA);
        Serial.println("[WIFI] Config mode exit: no saved WiFi.");
    }
}

bool WifiConfigManager::consumeWebSaveSuccess(String &ssid)
{
    if (!webSaveSuccessPending_)
    {
        return false;
    }

    ssid = lastWebSavedSsid_;
    lastWebSavedSsid_ = "";
    webSaveSuccessPending_ = false;
    return true;
}

bool WifiConfigManager::connectAndSaveFromLcd(const String &ssid, const String &pass)
{
    if (ssid.isEmpty())
    {
        return false;
    }

    Serial.printf("[WIFI] LCD connect request. SSID: %s\n", ssid.c_str());
    if (connectToSta(ssid, pass, 12000))
    {
        staReconnectInProgress_ = false;
        staReconnectTimedOut_ = false;
        staSsid_ = ssid;
        staPass_ = pass;
        saveConfig();

        apMode_ = false;
        WiFi.softAPdisconnect(true);
        Serial.printf("[WIFI] LCD connect success. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WIFI] LCD connect failed. Config not saved.");
    return false;
}

void WifiConfigManager::loadConfig()
{
    preferences_->begin("wifi_cfg", true);
    staSsid_ = preferences_->getString("ssid", "");
    staPass_ = preferences_->getString("pass", "");
    email_ = preferences_->getString("email", "");
    mqttUseCustom_ = preferences_->getBool("mqtt_custom", false);
    mqttHost_ = preferences_->getString("mqtt_host", kDefaultMqttHost);
    mqttPort_ = static_cast<uint16_t>(preferences_->getUInt("mqtt_port", kDefaultMqttPort));
    mqttUser_ = preferences_->getString("mqtt_user", "");
    mqttPass_ = preferences_->getString("mqtt_pass", "");
    preferences_->end();

    if (mqttPort_ == 0)
    {
        mqttPort_ = kDefaultMqttPort;
    }
}

void WifiConfigManager::saveConfig()
{
    preferences_->begin("wifi_cfg", false);
    preferences_->putString("ssid", staSsid_);
    preferences_->putString("pass", staPass_);
    preferences_->putString("email", email_);
    preferences_->putBool("mqtt_custom", mqttUseCustom_);
    preferences_->putString("mqtt_host", mqttHost_);
    preferences_->putUInt("mqtt_port", mqttPort_);
    preferences_->putString("mqtt_user", mqttUser_);
    preferences_->putString("mqtt_pass", mqttPass_);
    preferences_->end();
}

bool WifiConfigManager::connectToSta(const String &ssid, const String &pass, uint32_t timeoutMs)
{
    WiFi.mode(apMode_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    const unsigned long start = millis();
    while (millis() - start < timeoutMs)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            apMode_ = false;
            return true;
        }
        delay(200);
    }

    // Prevent lingering "sta is connecting" state from breaking WiFi scans.
    WiFi.disconnect(false, false);
    delay(40);
    return false;
}

void WifiConfigManager::processStaReconnect()
{
    if (!staReconnectInProgress_)
    {
        return;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        staReconnectInProgress_ = false;
        staReconnectTimedOut_ = false;
        Serial.printf("[WIFI] Async reconnect success. IP=%s\n", WiFi.localIP().toString().c_str());
        return;
    }

    if (millis() - staReconnectStartMs_ < staReconnectTimeoutMs_)
    {
        return;
    }

    staReconnectInProgress_ = false;
    staReconnectTimedOut_ = true;
    staReconnectTimeoutAtMs_ = millis();

    // Ensure WiFi leaves connecting state for future scan/connect operations.
    WiFi.disconnect(false, false);
    Serial.println("[WIFI] Async reconnect timeout. Staying OFFLINE.");
}

void WifiConfigManager::startAccessPoint()
{
    uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF);
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06X", chip);

    apSsid_ = "ESP32-";
    apSsid_ += suffix;

    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(apSsid_.c_str(), "12345678");
    apMode_ = ok;

    Serial.printf("[WIFI] AP mode started. SSID: %s, IP: %s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
}

void WifiConfigManager::startServer()
{
    server_->on("/", HTTP_GET, [this]()
                { handleRoot(); });
    server_->on("/scan", HTTP_GET, [this]()
                { handleScan(); });
    server_->on("/config", HTTP_GET, [this]()
                { handleGetConfig(); });
    server_->on("/save", HTTP_GET, [this]()
                { handleSave(); });

    server_->onNotFound([this]()
                        { server_->send(404, "text/plain", "Not found"); });

    server_->begin();
    Serial.printf("[WIFI] Web config server started at http://%s\n", ipAddress().c_str());
}

void WifiConfigManager::handleRoot()
{
    server_->send_P(200, "text/html", html_page);
}

void WifiConfigManager::handleScan()
{
    int networkCount = WiFi.scanNetworks(false, true);

    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < networkCount; ++i)
    {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty())
        {
            continue;
        }

        JsonObject item = arr.createNestedObject();
        item["ssid"] = ssid;
        item["rssi"] = WiFi.RSSI(i);
    }

    String response;
    serializeJson(arr, response);
    WiFi.scanDelete();
    server_->send(200, "application/json", response);
}

void WifiConfigManager::handleGetConfig()
{
    DynamicJsonDocument doc(1024);
    doc["ssid"] = staSsid_;
    doc["email"] = email_;
    doc["mqtt_use_custom"] = mqttUseCustom_;
    doc["mqtt_host"] = mqttHost();
    doc["mqtt_port"] = mqttPort();
    doc["mqtt_user"] = mqttUser();
    doc["mqtt_pass"] = mqttPass();
    doc["mqtt_default_host"] = kDefaultMqttHost;
    doc["mqtt_default_port"] = kDefaultMqttPort;
    doc["mqtt_default_user"] = kDefaultMqttUser;
    doc["mqtt_default_pass"] = kDefaultMqttPass;

    String response;
    serializeJson(doc, response);
    server_->send(200, "application/json", response);
}

void WifiConfigManager::handleSave()
{
    const String reqSsid = server_->arg("ssid");
    const String reqPass = server_->arg("pass");
    const String reqEmail = server_->arg("email");
    const bool reqMqttUseCustom = server_->arg("mqtt_use_custom") == "1";
    const String reqMqttHost = server_->arg("mqtt_host");
    const String reqMqttPort = server_->arg("mqtt_port");
    const String reqMqttUser = server_->arg("mqtt_user");
    const String reqMqttPass = server_->arg("mqtt_pass");

    if (reqSsid.isEmpty())
    {
        server_->send(400, "text/plain", "SSID required");
        return;
    }

    Serial.printf("[WIFI] Web save request. Try connect SSID: %s\n", reqSsid.c_str());

    if (connectToSta(reqSsid, reqPass, 12000))
    {
        staSsid_ = reqSsid;
        staPass_ = reqPass;
        email_ = reqEmail;
        mqttUseCustom_ = reqMqttUseCustom;

        long parsedPort = reqMqttPort.toInt();
        if (parsedPort < 1 || parsedPort > 65535)
        {
            parsedPort = kDefaultMqttPort;
        }

        if (mqttUseCustom_)
        {
            mqttHost_ = reqMqttHost.isEmpty() ? String(kDefaultMqttHost) : reqMqttHost;
            mqttPort_ = static_cast<uint16_t>(parsedPort);
            mqttUser_ = reqMqttUser;
            mqttPass_ = reqMqttPass;
        }
        else
        {
            mqttHost_ = kDefaultMqttHost;
            mqttPort_ = kDefaultMqttPort;
            mqttUser_ = kDefaultMqttUser;
            mqttPass_ = kDefaultMqttPass;
        }

        saveConfig();

        apMode_ = false;
        WiFi.softAPdisconnect(true);
        lastWebSavedSsid_ = staSsid_;
        webSaveSuccessPending_ = true;
        Serial.printf("[WIFI] Web save success. Connected to %s. IP: %s\n", staSsid_.c_str(), WiFi.localIP().toString().c_str());
        server_->send(200, "text/plain", "Saved");
    }
    else
    {
        Serial.println("[WIFI] Web save connect failed. Config not saved.");
        server_->send(400, "text/plain", "Connect failed");
    }
}
