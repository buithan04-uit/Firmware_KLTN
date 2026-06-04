#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_VL53L0X.h>
#include <math.h>
#include <esp_system.h>
#include <esp_rom_sys.h>
#include "ui.h"
#include "ui_datacollector.h"
#include "lcd_sensor_runtime.h"
#include "lcd_ui_presenter.h"
#include "wifi_config_manager.h"
#include "ad8232.h"
#include "mqtt_defaults.h"
#include <HTTPClient.h>
#include "boot_screen.h"

const String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbwY5kIR9FeXT1DND8rfqvesckEdptbt2v3hgehC8wbzYHp1_POFO8n5Qf0BMy1asSc/exec";

TFT_eSPI tft = TFT_eSPI();
bool in_menu = false;
static constexpr uint32_t SENSOR_UI_UPDATE_MS = 80;
static constexpr uint32_t ECG_SAMPLE_UPDATE_MS = 4;
static constexpr uint32_t ECG_UI_UPDATE_MS = 8;
static constexpr uint32_t ECG_LOG_INTERVAL_MS = 120;
static constexpr uint32_t ECG_IDLE_SAMPLE_MS = 30;
static constexpr uint32_t CONFIG_UI_UPDATE_MS = 1000;
static constexpr uint32_t WIFI_HEADER_UPDATE_MS = 800;
static constexpr uint8_t STATE_CONFIRM_TICKS = 3;
static constexpr uint32_t LIVE_LOG_INTERVAL_MS = 1500;
static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 3000;
static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 700;
static constexpr uint32_t MQTT_OK_LOG_INTERVAL_MS = 1500;
static constexpr uint8_t TP5100_CHRG_PIN = 34; // TLP521-2 output CHRG, active LOW
static constexpr uint8_t TP5100_FULL_PIN = 35; // TLP521-2 output STDBY/FULL, active LOW
static constexpr size_t MQTT_PAYLOAD_BUFFER = 384;
static constexpr uint8_t COLLECT_SAMPLE_LIMIT = 6;
static constexpr uint8_t COLLECT_PERSON_MAX = 7;
static constexpr uint8_t COLLECT_SESSION_MAX = 5;
static constexpr int COLLECT_MEASUREMENT_SAMPLES = 30;

static constexpr float COLLECT_DIST_MIN_MM = 40.0f;
static constexpr float COLLECT_DIST_MAX_MM = 50.0f;
static constexpr float COLLECT_DIST_TARGET_MM = 45.0f;

static constexpr float COLLECT_AMBIENT_MIN_C = 20.0f;
static constexpr float COLLECT_AMBIENT_MAX_C = 35.0f;
static constexpr float COLLECT_BODY_MIN_C = 32.0f;
static constexpr float COLLECT_BODY_MAX_C = 42.0f;

static constexpr float VL53_TO_MLX_OFFSET_MM = 18.0f;
static LcdSensorRuntime sensorRuntime;
static WifiConfigManager wifiConfigManager;
static TaskHandle_t ecgSamplingTaskHandle = nullptr;
static WiFiClient mqttNetClient;
static PubSubClient mqttClient(mqttNetClient);
static String mqttDeviceId;
static String mqttPublishTopic;
static SensorSnapshot latestMaxSnapshot;
static bool latestMaxLive = false;
static float latestEcgSample = 0.0f;
static bool latestEcgLive = false;
static bool mqttSendEnabled = false;
static unsigned long lastMqttReconnectAt = 0;
static unsigned long lastMqttPublishAt = 0;
static unsigned long lastMqttOkLogAt = 0;
static char mqttBrokerHost[64] = {0};
static uint16_t mqttBrokerPort = kDefaultMqttPort;
SemaphoreHandle_t i2c0Mutex = NULL;
static TaskHandle_t maxSamplingTaskHandle = nullptr;
static Adafruit_VL53L0X collectLox;
static bool collectLoxReady = false;
static bool collectIsMeasuring = false;
static int collectCurrentId = 1;
static int collectCurrentSession = 1;
static int collectSampleCount = 0;
static unsigned long collectLastLiveUpdate = 0;
static float collectLastDistance = 999.0f;
static bool collectPendingSend = false;
static float collectPendingDist = 0.0f;
static float collectPendingObj = 0.0f;
static float collectPendingAmb = 0.0f;
static int collectPendingPerson = 1;
static int collectPendingSession = 1;
static int collectPendingTrial = 1;
static bool measureAllPendingSend = false;
static float measureAllPendingDist = 0.0f;
static float measureAllPendingObj = 0.0f;
static float measureAllPendingAmb = 0.0f;
static float measureAllPendingEcg = 0.0f;
static int measureAllPendingHr = 0;
static int measureAllPendingSpo2 = 0;

static void updateTp5100ChargeStatus()
{
    const bool chargingActive = (digitalRead(TP5100_CHRG_PIN) == LOW);
    const bool fullActive = (digitalRead(TP5100_FULL_PIN) == LOW);
    sensorRuntime.setBatteryChargeStatus(chargingActive, fullActive);
}

__attribute__((constructor)) static void early_boot_log()
{
    ets_printf("[ROM] app_start\n");
}

static String buildDeviceId()
{
    const uint32_t tail = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF);
    char buf[20];
    snprintf(buf, sizeof(buf), "ESP32_%06X", tail);
    return String(buf);
}

static void syncMqttBrokerConfig(const WifiConfigManager &wifi)
{
    String host = wifi.mqttHost();
    if (!wifi.mqttUseCustom())
    {
        // In default mode, prefer current LAN gateway so broker on host PC/router is reachable
        // even when user network is not 192.168.1.x.
        IPAddress gw = WiFi.gatewayIP();
        if (gw[0] != 0 || gw[1] != 0 || gw[2] != 0 || gw[3] != 0)
        {
            host = gw.toString();
        }
        else if (host.isEmpty())
        {
            host = kDefaultMqttHost;
        }
    }
    else if (host.isEmpty())
    {
        host = kDefaultMqttHost;
    }
    const uint16_t port = wifi.mqttPort();

    bool changed = (port != mqttBrokerPort) || (strcmp(mqttBrokerHost, host.c_str()) != 0);
    if (!changed)
    {
        return;
    }

    snprintf(mqttBrokerHost, sizeof(mqttBrokerHost), "%s", host.c_str());
    mqttBrokerPort = port;
    mqttClient.setServer(mqttBrokerHost, mqttBrokerPort);
    if (mqttClient.connected())
    {
        mqttClient.disconnect();
    }

    Serial.printf("[MQTT] Broker config: %s:%u (%s)\n",
                  mqttBrokerHost,
                  static_cast<unsigned>(mqttBrokerPort),
                  wifi.mqttUseCustom() ? "custom" : "default");
}

static bool ensureMqttConnected(const WifiConfigManager &wifi)
{
    syncMqttBrokerConfig(wifi);

    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    if (mqttClient.connected())
    {
        return true;
    }

    const unsigned long now = millis();
    if (now - lastMqttReconnectAt < MQTT_RECONNECT_INTERVAL_MS)
    {
        return false;
    }
    lastMqttReconnectAt = now;

    const String mqttUser = wifi.mqttUser();
    const String mqttPass = wifi.mqttPass();
    if (mqttUser.isEmpty() || mqttPass.isEmpty())
    {
        Serial.println("[MQTT] Missing credentials in config page (mqtt_user/mqtt_pass).");
        return false;
    }

    const String clientId = String("lcd-") + mqttDeviceId;
    const bool connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str());
    if (connected)
    {
        Serial.printf("[MQTT] Connect %s:%u as %s => OK\n",
                      mqttBrokerHost,
                      static_cast<unsigned>(mqttBrokerPort),
                      mqttUser.c_str());
    }
    else
    {
        Serial.printf("[MQTT] Connect %s:%u as %s => FAILED (state=%d)\n",
                      mqttBrokerHost,
                      static_cast<unsigned>(mqttBrokerPort),
                      mqttUser.c_str(),
                      mqttClient.state());
    }
    return connected;
}

static void publishTelemetryIfReady(const WifiConfigManager &wifi, const LcdSensorRuntime &runtime, ScreenType activeScreen)
{
    if (!mqttSendEnabled)
    {
        return;
    }

    StaticJsonDocument<320> doc;
    const char *modeName = nullptr;
    bool hasField = false;

    switch (activeScreen)
    {
    case SCR_TEMP:
        modeName = "temp";
        if (runtime.mlxReady())
        {
            doc["temp"] = runtime.mlxBodyTempC();
            hasField = true;
        }
        break;

    case SCR_ECG:
        modeName = "ecg";
        if (latestEcgLive)
        {
            doc["ecg"] = latestEcgSample;
            hasField = true;
        }
        break;

    case SCR_SPO2:
        modeName = "spo2";
        if (latestMaxLive)
        {
            doc["hr"] = latestMaxSnapshot.heartRateBpm;
            doc["spo2"] = latestMaxSnapshot.spo2Percent;
            hasField = true;
        }
        break;

    case SCR_MONITOR:
        modeName = "monitor";
        if (latestMaxLive)
        {
            doc["hr"] = latestMaxSnapshot.heartRateBpm;
            doc["spo2"] = latestMaxSnapshot.spo2Percent;
            hasField = true;
        }
        if (runtime.mlxReady())
        {
            doc["temp"] = runtime.mlxBodyTempC();
            hasField = true;
        }
        break;

    case SCR_MEASUREALL:
        modeName = "measureall";
        if (latestMaxLive)
        {
            doc["hr"] = latestMaxSnapshot.heartRateBpm;
            doc["spo2"] = latestMaxSnapshot.spo2Percent;
            hasField = true;
        }
        if (runtime.mlxReady())
        {
            doc["temp"] = runtime.mlxBodyTempC();
            hasField = true;
        }
        if (latestEcgLive)
        {
            doc["ecg"] = latestEcgSample;
            hasField = true;
        }
        break;

    default:
        // Menu/config/wifi scan and other non-measurement screens: do not publish.
        return;
    }

    if (!hasField)
    {
        return;
    }

    const unsigned long now = millis();
    if (now - lastMqttPublishAt < MQTT_PUBLISH_INTERVAL_MS)
    {
        return;
    }
    lastMqttPublishAt = now;

    if (!ensureMqttConnected(wifi))
    {
        return;
    }

    doc["device_id"] = mqttDeviceId;
    doc["mode"] = modeName;
    doc["ts"] = now;

    char payload[MQTT_PAYLOAD_BUFFER];
    const size_t payloadLen = serializeJson(doc, payload, sizeof(payload));
    if (payloadLen == 0 || payloadLen >= sizeof(payload))
    {
        Serial.println("[MQTT] Payload encode failed or exceeded buffer.");
        return;
    }

    const bool ok = mqttClient.publish(mqttPublishTopic.c_str(), payload);
    if (!ok)
    {
        Serial.println("[MQTT] Publish failed.");
        return;
    }

    if (now - lastMqttOkLogAt >= MQTT_OK_LOG_INTERVAL_MS)
    {
        lastMqttOkLogAt = now;
        Serial.printf("[MQTT][%s] Publish OK topic=%s payload=%s\n",
                      modeName,
                      mqttPublishTopic.c_str(),
                      payload);
    }
}

static bool publishCollectData(const WifiConfigManager &wifi, float distMm, float objTemp, float ambTemp)
{
    if (!ensureMqttConnected(wifi))
    {
        return false;
    }

    StaticJsonDocument<320> doc;
    doc["device_id"] = mqttDeviceId;
    doc["mode"] = "collect";
    doc["ts"] = millis();
    doc["id"] = collectCurrentId;
    doc["sample"] = collectSampleCount + 1;
    doc["dist_mm"] = distMm;
    doc["temp"] = objTemp;
    doc["ambient"] = ambTemp;

    char payload[MQTT_PAYLOAD_BUFFER];
    size_t payloadLen = serializeJson(doc, payload, sizeof(payload));
    if (payloadLen == 0 || payloadLen >= sizeof(payload))
    {
        Serial.println("[MQTT] Collect payload encode failed or exceeded buffer.");
        return false;
    }

    bool ok = mqttClient.publish(mqttPublishTopic.c_str(), payload);
    if (ok)
    {
        Serial.printf("[MQTT][collect] Publish OK topic=%s payload=%s\n", mqttPublishTopic.c_str(), payload);
    }
    else
    {
        Serial.println("[MQTT][collect] Publish failed.");
    }
    return ok;
}

static bool publishMeasureAllData(const WifiConfigManager &wifi, float distMm, float objTemp, float ambTemp, int hr, int spo2, float ecg)
{
    if (!ensureMqttConnected(wifi))
    {
        return false;
    }

    StaticJsonDocument<320> doc;
    doc["device_id"] = mqttDeviceId;
    doc["mode"] = "measure_all";
    doc["ts"] = millis();
    doc["dist_mm"] = distMm;
    doc["temp"] = objTemp;
    doc["ambient"] = ambTemp;
    doc["hr"] = hr;
    doc["spo2"] = spo2;
    doc["ecg"] = ecg;

    char payload[MQTT_PAYLOAD_BUFFER];
    size_t payloadLen = serializeJson(doc, payload, sizeof(payload));
    if (payloadLen == 0 || payloadLen >= sizeof(payload))
    {
        Serial.println("[MQTT] MeasureAll payload encode failed or exceeded buffer.");
        return false;
    }

    bool ok = mqttClient.publish(mqttPublishTopic.c_str(), payload);
    if (ok)
    {
        Serial.printf("[MQTT][measure_all] Publish OK topic=%s payload=%s\n", mqttPublishTopic.c_str(), payload);
    }
    else
    {
        Serial.println("[MQTT][measure_all] Publish failed.");
    }
    return ok;
}

template <typename TState>
struct StateTracker
{
    TState stable;
    TState candidate;
    uint8_t confirmCount;
};

static bool update_stable_state(StateTracker<MaxRuntimeState> &tracker, MaxRuntimeState rawState)
{
    if (rawState == tracker.stable)
    {
        tracker.candidate = rawState;
        tracker.confirmCount = 0;
        return false;
    }

    if (rawState != tracker.candidate)
    {
        tracker.candidate = rawState;
        tracker.confirmCount = 1;
        return false;
    }

    if (tracker.confirmCount < STATE_CONFIRM_TICKS)
    {
        tracker.confirmCount++;
    }

    if (tracker.confirmCount >= STATE_CONFIRM_TICKS)
    {
        tracker.stable = rawState;
        tracker.confirmCount = 0;
        return true;
    }

    return false;
}

static bool update_stable_state(StateTracker<EcgRuntimeState> &tracker, EcgRuntimeState rawState)
{
    if (rawState == tracker.stable)
    {
        tracker.candidate = rawState;
        tracker.confirmCount = 0;
        return false;
    }

    if (rawState != tracker.candidate)
    {
        tracker.candidate = rawState;
        tracker.confirmCount = 1;
        return false;
    }

    if (tracker.confirmCount < STATE_CONFIRM_TICKS)
    {
        tracker.confirmCount++;
    }

    if (tracker.confirmCount >= STATE_CONFIRM_TICKS)
    {
        tracker.stable = rawState;
        tracker.confirmCount = 0;
        return true;
    }

    return false;
}

static bool is_max30102_screen(ScreenType scr)
{
    return (scr == SCR_MONITOR || scr == SCR_SPO2);
}

static bool is_ecg_screen(ScreenType scr)
{
    return (scr == SCR_ECG);
}

static bool is_max_sampling_screen(ScreenType scr)
{
    return (scr == SCR_MONITOR || scr == SCR_SPO2 || scr == SCR_MEASUREALL);
}

static bool is_ecg_sampling_screen(ScreenType scr)
{
    return (scr == SCR_ECG || scr == SCR_MEASUREALL);
}

static bool is_temp_screen(ScreenType scr)
{
    return (scr == SCR_TEMP);
}

static bool is_publish_screen(ScreenType scr)
{
    return (scr == SCR_MONITOR || scr == SCR_ECG || scr == SCR_SPO2 || scr == SCR_TEMP || scr == SCR_MEASUREALL);
}

static const char *screen_name(ScreenType scr)
{
    switch (scr)
    {
    case SCR_MONITOR:
        return "MONITOR";
    case SCR_ECG:
        return "ECG";
    case SCR_SPO2:
        return "SPO2";
    case SCR_TEMP:
        return "TEMP";
    case SCR_COLLECTDATA:
        return "COLLECT";
    case SCR_MEASUREALL:
        return "MEASURE_ALL";
    case SCR_MENU:
        return "MENU";
    case SCR_CONFIG:
        return "CONFIG";
    case SCR_WIFI_SCAN:
        return "WIFI_SCAN";
    case SCR_WIFI_PASS:
        return "WIFI_PASS";
    default:
        return "BOOT";
    }
}

static void refresh_mqtt_ui_status(ScreenType screen)
{
    if (!is_publish_screen(screen))
    {
        ui_set_mqtt_status("-", 0x666666);
        return;
    }

    if (mqttSendEnabled)
    {
        ui_set_mqtt_status("SEND ON", 0x00E676);
    }
    else
    {
        ui_set_mqtt_status("SEND OFF", 0xFFB300);
    }
}

static void refresh_collect_wifi_ui(WifiConfigManager &wifi)
{
    String ssid = "OFF";
    String level = "0/4";
    uint32_t color = 0xFF5252;

    if (wifi.isStaReconnectInProgress())
    {
        ssid = wifi.staReconnectSsid();
        if (ssid.isEmpty())
        {
            ssid = "RECONNECT";
        }
        level = "...";
        color = 0xFFB300;
        ui_datacollector_update_wifi_detail(level.c_str(), ssid.c_str(), color);
        return;
    }

    if (wifi.hasRecentStaReconnectTimeout())
    {
        ssid = "TIMEOUT";
        level = "!";
        color = 0xFF5252;
        ui_datacollector_update_wifi_detail(level.c_str(), ssid.c_str(), color);
        return;
    }

    if (wifi.isApMode())
    {
        ssid = wifi.apSsid();
        level = "AP";
        color = 0xFFB300;
    }
    else if (wifi.isConnected())
    {
        ssid = wifi.connectedSsid();
        int rssi = wifi.signalRssi();

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

    ui_datacollector_update_wifi_detail(level.c_str(), ssid.c_str(), color);
}

// Hàm in bộ nhớ trống
void print_heap()
{
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" | Min Free Heap: ");
    Serial.println(ESP.getMinFreeHeap());
}

static String collect_session_id(int personId, int sessionId)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "P%02d_S%02d", personId, sessionId);
    return String(buf);
}

static String collect_person_id_text(int personId)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "P%02d", personId);
    return String(buf);
}

static const char *collect_trial_phase(int trial)
{
    return (trial <= 3) ? "pre_omron" : "post_omron";
}

static const char *collect_distance_band(float distMm)
{
    if (!isfinite(distMm) || distMm >= 999.0f)
    {
        return "invalid";
    }

    if (distMm < 43.0f)
    {
        return "40_42";
    }

    if (distMm <= 47.0f)
    {
        return "44_46";
    }

    return "48_50";
}

static void collect_refresh_subject_ui()
{
    ui_datacollector_update_session(collectCurrentId, collectCurrentSession);
    ui_datacollector_update_progress(collectSampleCount, COLLECT_SAMPLE_LIMIT);
}

static float collectMedBuf[3] = {0.0f, 0.0f, 0.0f};
static int collectMedIdx = 0;
static bool collectMedFilled = false;
static float collectEmaValue = 0.0f;
static bool collectEmaInit = false;

static float collect_median3(float newVal)
{
    collectMedBuf[collectMedIdx] = newVal;
    collectMedIdx = (collectMedIdx + 1) % 3;
    if (!collectMedFilled && collectMedIdx == 0)
    {
        collectMedFilled = true;
    }
    if (!collectMedFilled)
    {
        return newVal;
    }

    float a = collectMedBuf[0];
    float b = collectMedBuf[1];
    float c = collectMedBuf[2];
    if ((a >= b && a <= c) || (a <= b && a >= c))
    {
        return a;
    }
    if ((b >= a && b <= c) || (b <= a && b >= c))
    {
        return b;
    }
    return c;
}

static const int COLLECT_CAL_POINTS = 12;
static const float collectRawPts[COLLECT_CAL_POINTS] = {
    36, 46, 54, 64, 76, 87, 100, 116, 132, 152, 178, 229};
static const float collectActualPts[COLLECT_CAL_POINTS] = {
    10, 20, 28, 38, 50, 62, 76, 92, 108, 128, 153, 205};

static float collect_interpolate(float raw)
{
    if (raw <= collectRawPts[0])
    {
        float slope = (collectActualPts[1] - collectActualPts[0]) / (collectRawPts[1] - collectRawPts[0]);
        float result = collectActualPts[0] + slope * (raw - collectRawPts[0]);
        return (result < 0.0f) ? 0.0f : result;
    }
    if (raw >= collectRawPts[COLLECT_CAL_POINTS - 1])
    {
        float slope = (collectActualPts[COLLECT_CAL_POINTS - 1] - collectActualPts[COLLECT_CAL_POINTS - 2]) /
                      (collectRawPts[COLLECT_CAL_POINTS - 1] - collectRawPts[COLLECT_CAL_POINTS - 2]);
        return collectActualPts[COLLECT_CAL_POINTS - 1] + slope * (raw - collectRawPts[COLLECT_CAL_POINTS - 1]);
    }
    for (int i = 0; i < COLLECT_CAL_POINTS - 1; i++)
    {
        if (raw <= collectRawPts[i + 1])
        {
            float t = (raw - collectRawPts[i]) / (collectRawPts[i + 1] - collectRawPts[i]);
            return collectActualPts[i] + t * (collectActualPts[i + 1] - collectActualPts[i]);
        }
    }
    return raw - 24.0f;
}

static float collect_ema(float newVal)
{
    if (!collectEmaInit)
    {
        collectEmaValue = newVal;
        collectEmaInit = true;
        return collectEmaValue;
    }

    float diff = fabsf(newVal - collectEmaValue);
    float alpha = 0.15f;
    if (diff >= 15.0f)
    {
        alpha = 0.85f;
    }
    else if (diff > 5.0f)
    {
        float t = (diff - 5.0f) / (15.0f - 5.0f);
        alpha = 0.15f + t * (0.85f - 0.15f);
    }

    collectEmaValue = alpha * newVal + (1.0f - alpha) * collectEmaValue;
    return collectEmaValue;
}

static float collect_corrected_distance(float rawDist)
{
    float clean = collect_median3(rawDist);
    return collect_interpolate(clean);
}

static bool collect_distance_valid(float dist)
{
    return dist >= COLLECT_DIST_MIN_MM && dist <= COLLECT_DIST_MAX_MM;
}

static bool collect_temp_valid(float objTemp, float ambTemp)
{
    if (!isfinite(objTemp) || !isfinite(ambTemp))
    {
        return false;
    }
    if (ambTemp < COLLECT_AMBIENT_MIN_C || ambTemp > COLLECT_AMBIENT_MAX_C)
    {
        return false;
    }
    return objTemp >= COLLECT_BODY_MIN_C && objTemp <= COLLECT_BODY_MAX_C;
}

static float collect_mean(const float *data, int n)
{
    if (n <= 0)
    {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        sum += data[i];
    }
    return sum / n;
}

static void collect_recover_i2c()
{
    Serial.println("[COLLECT] VL53L0X reset I2C");
    Wire.end();
    delay(50);
    Wire.begin(21, 22);
    Wire.setClock(50000);
    Wire.setTimeOut(500);
    collectLox.begin();
}

static bool collect_ensure_lox_ready()
{
    if (collectLoxReady)
    {
        return true;
    }

    if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }
    bool ok = collectLox.begin();
    xSemaphoreGive(i2c0Mutex);
    collectLoxReady = ok;
    if (ok)
    {
        Serial.println("[COLLECT] VL53L0X ready");
    }
    else
    {
        Serial.println("[COLLECT] VL53L0X not detected");
    }
    return ok;
}

static bool collect_read_raw_distance(uint16_t &raw)
{
    if (!collectLoxReady)
    {
        return false;
    }

    if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(20)) != pdTRUE)
    {
        return false;
    }

    VL53L0X_RangingMeasurementData_t measure;
    collectLox.rangingTest(&measure, false);
    if (measure.RangeStatus == 4 || measure.RangeMilliMeter == 8190)
    {
        collect_recover_i2c();
        xSemaphoreGive(i2c0Mutex);
        return false;
    }

    raw = measure.RangeMilliMeter;
    xSemaphoreGive(i2c0Mutex);
    return true;
}

static float collect_mlx_target_distance(float vl53CorrectedDist)
{
    if (!isfinite(vl53CorrectedDist) || vl53CorrectedDist >= 999.0f)
    {
        return 999.0f;
    }

    float mlxDist = vl53CorrectedDist - VL53_TO_MLX_OFFSET_MM;

    if (mlxDist < 0.0f)
    {
        mlxDist = 0.0f;
    }

    return mlxDist;
}

static float collect_live_distance()
{
    uint16_t raw = 0;
    if (!collect_read_raw_distance(raw))
    {
        return 999.0f;
    }

    const float vl53Dist = collect_corrected_distance(static_cast<float>(raw));
    const float mlxDist = collect_mlx_target_distance(vl53Dist);

    return collect_ema(mlxDist);
}

static void collect_perform_measurement(const WifiConfigManager &wifi)
{
    if (collectPendingSend)
    {
        return;
    }

    if (collectIsMeasuring)
    {
        return;
    }

    if (collectSampleCount >= COLLECT_SAMPLE_LIMIT)
    {
        ui_datacollector_show_popup("WARNING", "Sample limit reached",
                                    lv_palette_main(LV_PALETTE_RED), false);
        delay(1000);
        ui_datacollector_hide_popup();
        return;
    }

    if (!collect_ensure_lox_ready())
    {
        ui_datacollector_show_popup("SENSOR", "VL53L0X not ready",
                                    lv_palette_main(LV_PALETTE_RED), false);
        delay(1000);
        ui_datacollector_hide_popup();
        return;
    }

    collectIsMeasuring = true;
    ui_datacollector_show_popup("MEASURING", "Collecting samples...",
                                lv_palette_main(LV_PALETTE_BLUE), true);

    float distSamples[COLLECT_MEASUREMENT_SAMPLES];
    float objSamples[COLLECT_MEASUREMENT_SAMPLES];
    float ambSamples[COLLECT_MEASUREMENT_SAMPLES];
    int count = 0;
    int loop_timeout = 0;

    while (count < COLLECT_MEASUREMENT_SAMPLES && loop_timeout < 150)
    {
        loop_timeout++;
        VL53L0X_RangingMeasurementData_t measure;
        bool valid = false;

        if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            sensorRuntime.updateBackground(true);
            collectLox.rangingTest(&measure, false);
            if (measure.RangeStatus != 4 && measure.RangeMilliMeter != 8190)
            {
                valid = true;
            }
            else
            {
                collect_recover_i2c();
            }
            xSemaphoreGive(i2c0Mutex);
        }

        if (valid)
        {
            float vl53Dist = collect_corrected_distance(static_cast<float>(measure.RangeMilliMeter));
            float mlxDist = collect_mlx_target_distance(vl53Dist);
            float objTemp = sensorRuntime.mlxBodyTempC();
            float ambTemp = sensorRuntime.mlxAmbientTempC();

            if (collect_distance_valid(mlxDist) && collect_temp_valid(objTemp, ambTemp))
            {
                distSamples[count] = mlxDist;
                objSamples[count] = objTemp;
                ambSamples[count] = ambTemp;
                count++;
            }
        }

        ui_datacollector_update_popup_progress((count * 100) / COLLECT_MEASUREMENT_SAMPLES);
        lv_timer_handler();
        delay(20);
    }

    // Xử lý nếu hết thời gian mà vẫn không lấy đủ mẫu
    if (count == 0)
    {
        ui_datacollector_show_popup("LỖI", "Không đo được khoảng cách chuẩn!", lv_palette_main(LV_PALETTE_RED), false);
        delay(1000);
        ui_datacollector_hide_popup();
        collectIsMeasuring = false;
        return;
    }

    float avgD = collect_mean(distSamples, count);
    float avgO = collect_mean(objSamples, count);
    float avgA = collect_mean(ambSamples, count);

    collectPendingSend = true;
    collectPendingDist = avgD;
    collectPendingObj = avgO;
    collectPendingAmb = avgA;
    collectPendingPerson = collectCurrentId;
    collectPendingSession = collectCurrentSession;
    collectPendingTrial = collectSampleCount + 1;

    char msg[160];
    snprintf(msg, sizeof(msg),
             "P%02d S%02d T%d/%d\nDist: %.0f mm Obj: %.2f C\nAmb: %.2f C\nENTER=Send | DOWN=Cancel",
             collectPendingPerson,
             collectPendingSession,
             collectPendingTrial,
             COLLECT_SAMPLE_LIMIT,
             avgD,
             avgO,
             avgA);
    ui_datacollector_show_popup("RESULT - CONFIRM SEND!", msg,
                                lv_palette_main(LV_PALETTE_GREEN), false);
    collectIsMeasuring = false;
}

static void measure_all_once(const WifiConfigManager &wifi)
{
    ui_set_measure_all_status("SENDING...", 0xFFB300);

    bool sent = publishMeasureAllData(wifi,
                                      measureAllPendingDist,
                                      measureAllPendingObj,
                                      measureAllPendingAmb,
                                      measureAllPendingHr,
                                      measureAllPendingSpo2,
                                      measureAllPendingEcg);
    if (sent)
    {
        ui_set_measure_all_status("SEND OK", 0x00E676);
    }
    else
    {
        ui_set_measure_all_status("SEND FAILED", 0xFF5252);
    }

    measureAllPendingSend = false;
}

static void measure_all_prepare()
{
    float distMm = 999.0f;
    uint16_t raw = 0;
    if (collect_read_raw_distance(raw))
    {
        const float vl53Dist = collect_corrected_distance(static_cast<float>(raw));
        distMm = collect_mlx_target_distance(vl53Dist);
    }

    float objTemp = sensorRuntime.mlxReady() ? sensorRuntime.mlxBodyTempC() : -1.0f;
    float ambTemp = sensorRuntime.mlxConnected() ? sensorRuntime.mlxAmbientTempC() : -999.0f;
    SensorSnapshot maxSnap = sensorRuntime.maxSnapshot();
    SensorSnapshot ecgSnap = sensorRuntime.ecgSnapshot();

    int hr = maxSnap.signalReady ? maxSnap.heartRateBpm : 0;
    int spo2 = maxSnap.signalReady ? maxSnap.spo2Percent : 0;
    bool ecgLive = sensorRuntime.ad8232Ready() && ecgSnap.sensorReady && ecgSnap.signalReady;
    float ecgVal = ecgLive ? getECGFilteredSignal() : 0.0f;

    measureAllPendingDist = distMm;
    measureAllPendingObj = objTemp;
    measureAllPendingAmb = ambTemp;
    measureAllPendingHr = hr;
    measureAllPendingSpo2 = spo2;
    measureAllPendingEcg = ecgVal;
    measureAllPendingSend = true;

    ui_set_measure_all_values(objTemp, hr, spo2, ecgVal, distMm);
    ui_set_measure_all_status("CONFIRM: ENTER TO SEND", 0xFFB300);
}

void ecgSamplingTask(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();

    while (1)
    {
        const bool ecgActive = in_menu && is_ecg_sampling_screen(current_screen_type);

        // Keep ECG sampling at 250Hz only when ECG screen is active.
        // This prevents ADS1115/I2C retries from degrading menu/dashboard responsiveness.
        sensorRuntime.updateEcgBackground(ecgActive);

        const TickType_t periodTicks = pdMS_TO_TICKS(ecgActive ? ECG_SAMPLE_UPDATE_MS : ECG_IDLE_SAMPLE_MS);
        vTaskDelayUntil(&lastWake, periodTicks);
    }
}

void maxSamplingTask(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();

    while (1)
    {
        // Chỉ chạy MAX30102 nếu đang ở màn hình Monitor hoặc SpO2
        if (in_menu && is_max_sampling_screen(current_screen_type) && sensorRuntime.maxReady())
        {
            // Xin chìa khóa I2C0, chờ tối đa 10ms
            if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                sensorRuntime.updateMax30102();
                xSemaphoreGive(i2c0Mutex); // Đọc xong trả ngay
            }
        }

        // Tần số quét 100Hz (10ms)
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10));
    }
}

void guiTask(void *pvParameters)
{
    Serial.println("[BOOT] guiTask start");
    // Tạo sampling tasks trước — chúng sẽ chờ sensor được init trong boot sequence bên dưới
    if (ecgSamplingTaskHandle == nullptr)
    {
        xTaskCreatePinnedToCore(ecgSamplingTask, "ECG-SAMPLER", 4096, NULL, 3, &ecgSamplingTaskHandle, 0); // Core 0, Priority 3
    }
    if (maxSamplingTaskHandle == nullptr)
    {
        xTaskCreatePinnedToCore(maxSamplingTask, "MAX-SAMPLER", 4096, NULL, 2, &maxSamplingTaskHandle, 0); // Core 0, Priority 2
    }

    Serial.println("GUI Task Started");
    ui_init();
    Serial.println("[BOOT] ui_init done");
    lv_timer_handler();

    wifiConfigManager.begin();
    boot_screen_set_progress(20);
    lv_timer_handler();

    // Init sensors tại đây (sau khi boot screen đã render) để badge nhận đúng kết quả
    sensorRuntime.begin();
    updateTp5100ChargeStatus();
    boot_screen_set_sensor_status(BOOT_SENSOR_AD8232,
                                  sensorRuntime.ad8232Ready() ? BOOT_SENSOR_OK : BOOT_SENSOR_RETRY);
    boot_screen_set_progress(40);
    lv_timer_handler();

    // MAX30102: probe tại boot để hiện status ngay
    {
        bool maxOk = sensorRuntime.beginMax30102();
        boot_screen_set_sensor_status(BOOT_SENSOR_MAX30102,
                                      maxOk ? BOOT_SENSOR_OK : BOOT_SENSOR_RETRY);
    }
    boot_screen_set_sensor_status(BOOT_SENSOR_MLX90614,
                                  sensorRuntime.mlxConnected() ? BOOT_SENSOR_OK : BOOT_SENSOR_RETRY);
    boot_screen_set_progress(65);
    lv_timer_handler();

    // VL53L0X: probe I2C address để hiện status; lazy-init đầy đủ khi vào screen Collect
    {
        bool loxOk = collect_ensure_lox_ready();
        boot_screen_set_sensor_status(BOOT_SENSOR_VL53L0X,
                                      loxOk ? BOOT_SENSOR_OK : BOOT_SENSOR_RETRY);
    }
    boot_screen_set_sensor_status(BOOT_SENSOR_WIFI,
                                  wifiConfigManager.isConnected() ? BOOT_SENSOR_OK : BOOT_SENSOR_OFF);
    boot_screen_set_progress(100);
    lv_timer_handler();

    mqttDeviceId = buildDeviceId();
    mqttPublishTopic = String("vitals/") + mqttDeviceId + "/data";
    syncMqttBrokerConfig(wifiConfigManager);
    mqttClient.setKeepAlive(20);
    mqttClient.setSocketTimeout(3);
    mqttClient.setBufferSize(MQTT_PAYLOAD_BUFFER);
    Serial.printf("[MQTT] Device ID: %s\n", mqttDeviceId.c_str());
    Serial.printf("[MQTT] Publish topic: %s\n", mqttPublishTopic.c_str());
    Serial.printf("[MQTT] Broker: %s:%u\n", mqttBrokerHost, static_cast<unsigned>(mqttBrokerPort));

    Serial.println("System Started. Waiting 3s...");
    print_heap(); // Kiểm tra RAM lúc mới khởi động

    unsigned long startBoot = millis();
    StateTracker<MaxRuntimeState> maxStateTracker{MaxRuntimeState::NoSensor, MaxRuntimeState::NoSensor, 0};
    StateTracker<EcgRuntimeState> ecgStateTracker{EcgRuntimeState::NoSensor, EcgRuntimeState::NoSensor, 0};
    ScreenType lastScreen = SCR_BOOT;
    unsigned long lastMaxLiveLog = 0;
    unsigned long lastEcgLiveLog = 0;
    unsigned long lastConfigUiUpdate = 0;
    unsigned long lastWifiHeaderUpdate = 0;
    unsigned long configAutoBackAt = 0;
    unsigned long lastTempDiagLog = 0;
    unsigned long lastEcgUiFrameAt = 0;
    bool lastMlxReady = sensorRuntime.mlxReady();

    while (1)
    {
        lv_timer_handler();
        wifiConfigManager.update();
        mqttClient.loop();
        const bool mlxSamplingRequired = is_temp_screen(current_screen_type) ||
                                         (current_screen_type == SCR_MONITOR) ||
                                         (current_screen_type == SCR_COLLECTDATA) ||
                                         (current_screen_type == SCR_MEASUREALL);
        updateTp5100ChargeStatus();
        if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            sensorRuntime.updateBackground(mlxSamplingRequired);
            xSemaphoreGive(i2c0Mutex);
        }

        // Keep scheduler responsive so ECG background sampling can stay close to 250Hz.
        delay(1);

        // LOGIC CHUYỂN MÀN HÌNH
        if (!in_menu && (millis() - startBoot > 3000))
        {
            in_menu = true;
            Serial.println("--- BEFORE SWITCHING ---");
            print_heap();

            Serial.println("Switching to Main Menu...");
            ui_switch_screen(SCR_MENU);

            Serial.println("--- AFTER SWITCHING ---");
            print_heap(); // <--- Xem RAM còn bao nhiêu sau khi tạo menu
        }

        if (current_screen_type != lastScreen)
        {
            if (is_publish_screen(current_screen_type))
            {
                mqttSendEnabled = false;
                Serial.printf("[MQTT][CTRL] Enter %s: SEND=OFF. Press ENTER to start sending.\n", screen_name(current_screen_type));
            }
            else
            {
                mqttSendEnabled = false;
            }
            refresh_mqtt_ui_status(current_screen_type);

            if (lastScreen == SCR_CONFIG && current_screen_type != SCR_CONFIG)
            {
                Serial.println("[WIFI] Leaving config screen.");
                wifiConfigManager.exitConfigMode();
                configAutoBackAt = 0;
            }

            if (lastScreen == SCR_COLLECTDATA && current_screen_type != SCR_COLLECTDATA)
            {
                Serial.println("[COLLECT] Exiting collect data screen - cleanup");
                collectPendingSend = false;
                ui_datacollector_cleanup();
                collectIsMeasuring = false;
            }

            if (lastScreen == SCR_MEASUREALL && current_screen_type != SCR_MEASUREALL)
            {
                measureAllPendingSend = false;
            }

            if (is_max30102_screen(current_screen_type))
            {
                Serial.println("[MAX30102] Entered sensor screen, checking device...");
                bool max30102Ready = sensorRuntime.beginMax30102();

                if (max30102Ready)
                {
                    Serial.println("[MAX30102] Sensor detected.");
                    notify_max_state(MaxRuntimeState::WaitingFinger,
                                     current_screen_type == SCR_MONITOR,
                                     sensorRuntime.maxReady(),
                                     sensorRuntime.mlxReady());
                    maxStateTracker = {MaxRuntimeState::WaitingFinger, MaxRuntimeState::WaitingFinger, 0};
                }
                else
                {
                    Serial.println("[MAX30102] Sensor NOT detected.");
                    notify_max_state(MaxRuntimeState::NoSensor,
                                     current_screen_type == SCR_MONITOR,
                                     sensorRuntime.maxReady(),
                                     sensorRuntime.mlxReady());
                    maxStateTracker = {MaxRuntimeState::NoSensor, MaxRuntimeState::NoSensor, 0};
                }
            }
            else if (is_ecg_screen(current_screen_type))
            {
                Serial.println("[ECG] Entered ECG screen, checking AD8232/ADS1115...");
                lastEcgUiFrameAt = 0;
                ui_update_ecg_live(0.0f, 0, false);
                bool ad8232Ready = sensorRuntime.beginAd8232();

                if (ad8232Ready)
                {
                    Serial.println("[ECG] AD8232/ADS1115 detected.");
                    notify_ecg_state(EcgRuntimeState::LeadsOff);
                    ecgStateTracker = {EcgRuntimeState::LeadsOff, EcgRuntimeState::LeadsOff, 0};
                }
                else
                {
                    Serial.println("[ECG] AD8232/ADS1115 NOT detected.");
                    notify_ecg_state(EcgRuntimeState::NoSensor);
                    ecgStateTracker = {EcgRuntimeState::NoSensor, EcgRuntimeState::NoSensor, 0};
                }
            }
            else if (current_screen_type == SCR_CONFIG)
            {
                Serial.println("[WIFI] Entered config screen.");
                wifiConfigManager.forceStartAp();
                refresh_config_ui(wifiConfigManager);
                configAutoBackAt = 0;
            }
            else if (current_screen_type == SCR_COLLECTDATA)
            {
                Serial.println("[COLLECT] Entered collect data screen.");
                collectIsMeasuring = false;
                collect_ensure_lox_ready();
                collect_refresh_subject_ui();
                refresh_collect_wifi_ui(wifiConfigManager);
            }
            else if (current_screen_type == SCR_MEASUREALL)
            {
                Serial.println("[MEASURE_ALL] Entered measure all screen.");
                collect_ensure_lox_ready();
                sensorRuntime.beginMax30102();
                ui_set_measure_all_status("ENTER: TOGGLE SEND", 0xAAAAAA);
            }

            refresh_wifi_header_ui(wifiConfigManager);
            ui_set_battery(sensorRuntime.batteryPercent(), sensorRuntime.batteryCharging(), sensorRuntime.batteryFull());
            lastScreen = current_screen_type;
        }

        if (in_menu && is_ecg_screen(current_screen_type) && (millis() - lastEcgUiFrameAt >= ECG_UI_UPDATE_MS))
        {
            lastEcgUiFrameAt = millis();

            const SensorSnapshot e = sensorRuntime.ecgSnapshot();
            const bool ecgLive = sensorRuntime.ad8232Ready() && e.sensorReady && e.signalReady;
            latestEcgLive = ecgLive;

            if (!ecgLive)
            {
                latestEcgSample = 0.0f;
                ui_update_ecg_live(100.0f, 0, false); // đường phẳng giữa khi leads off
            }
            else
            {
                // Dùng filteredSignal: đã qua HPF+LPF+notch trong ad8232.cpp
                // Output là tín hiệu AC thuần (DC đã bị HPF loại), đơn vị mV
                const float filt = getECGFilteredSignal();

                // Kiểm tra mẫu mới để tránh vẽ lại khi I2C stall
                static float lastDrawFilt = -99999.0f;
                if (filt == lastDrawFilt)
                {
                    goto ecg_ui_done; // không có mẫu mới, bỏ qua frame này
                }
                lastDrawFilt = filt;
                latestEcgSample = filt;

                // FIX: KHÔNG normalize ecgEnvelope ở đây nữa.
                // Trước đây lcd.cpp normalize filt→chartY rồi ui.cpp lại envelope+normalize lần 2.
                // Double-normalize làm xẹp đỉnh QRS nghiêm trọng.
                // Bây giờ: pass thẳng filt (mV) vào ui_update_ecg_live, để ui.cpp xử lý 1 lần duy nhất.
                ui_update_ecg_live(filt, e.heartRateBpm, true);

                if (millis() - lastEcgLiveLog >= ECG_LOG_INTERVAL_MS)
                {
                    lastEcgLiveLog = millis();
                    // Log filt trực tiếp để debug; chartY do ui.cpp tính
                    Serial.printf("[ECG] Raw=%.2f Filt=%.2f HR=%d\n",
                                  getECGRawSignal(), filt, e.heartRateBpm);
                }
            }
        ecg_ui_done:;
        }

        static unsigned long lastUpdate = 0;
        if (in_menu && (millis() - lastUpdate > SENSOR_UI_UPDATE_MS))
        {
            lastUpdate = millis();

            if (millis() - lastWifiHeaderUpdate >= WIFI_HEADER_UPDATE_MS)
            {
                lastWifiHeaderUpdate = millis();
                refresh_wifi_header_ui(wifiConfigManager);
                if (current_screen_type == SCR_COLLECTDATA)
                {
                    refresh_collect_wifi_ui(wifiConfigManager);
                }
            }

            // Update battery indicator every 2 seconds (works on all screens including boot)
            static unsigned long lastBatteryUpdate = 0;
            if (millis() - lastBatteryUpdate >= 2000)
            {
                lastBatteryUpdate = millis();
                ui_set_battery(
                    sensorRuntime.batteryPercent(),
                    sensorRuntime.batteryCharging(),
                    sensorRuntime.batteryFull());

                const INA219Snapshot bat = sensorRuntime.ina219Snapshot();
                Serial.printf("[BAT] ready=%d V=%.3f I=%.1fmA P=%d%% chg=%d full=%d present=%d\n",
                              sensorRuntime.ina219Ready() ? 1 : 0,
                              bat.busVoltageV,
                              bat.currentMa,
                              bat.batteryPercent,
                              bat.isCharging ? 1 : 0,
                              bat.isFull ? 1 : 0,
                              bat.chargerPresent ? 1 : 0);
            }

            if (lastMlxReady != sensorRuntime.mlxReady())
            {
                lastMlxReady = sensorRuntime.mlxReady();
                if (current_screen_type == SCR_MONITOR)
                {
                    notify_max_state(maxStateTracker.stable, true, sensorRuntime.maxReady(), sensorRuntime.mlxReady());
                }
                else if (current_screen_type == SCR_TEMP)
                {
                    if (sensorRuntime.mlxReady())
                    {
                        ui_set_sensor_status("MLX90614 LIVE", 0x00E676);
                    }
                    else if (sensorRuntime.mlxConnected())
                    {
                        ui_set_sensor_status("MLX90614 DANG DO...", 0xFFB300);
                    }
                    else
                    {
                        ui_set_sensor_status("NO MLX90614 - CHECK WIRING", 0xFF5252);
                    }
                }
            }

            String reqSsid;
            String reqPass;
            if (ui_consume_wifi_connect_request(reqSsid, reqPass))
            {
                bool connected = wifiConfigManager.connectAndSaveFromLcd(reqSsid, reqPass);
                if (connected)
                {
                    ui_set_wifi_connect_feedback("CONNECTED - OPEN CONFIG PAGE", 0x00E676);
                    Serial.printf("[WIFI] LCD connect success: %s\n", reqSsid.c_str());
                }
                else
                {
                    ui_set_wifi_connect_feedback("CONNECT FAILED - CHECK PASSWORD", 0xFF5252);
                    Serial.printf("[WIFI] LCD connect failed: %s\n", reqSsid.c_str());
                }
            }

            if (ui_consume_mqtt_send_toggle_request())
            {
                if (is_publish_screen(current_screen_type))
                {
                    mqttSendEnabled = !mqttSendEnabled;
                    Serial.printf("[MQTT][CTRL] %s SEND => %s\n",
                                  screen_name(current_screen_type),
                                  mqttSendEnabled ? "ON" : "OFF");
                    refresh_mqtt_ui_status(current_screen_type);
                }
                else
                {
                    Serial.printf("[MQTT][CTRL] Ignore toggle on %s\n", screen_name(current_screen_type));
                    refresh_mqtt_ui_status(current_screen_type);
                }
            }

            if (current_screen_type == SCR_CONFIG)
            {
                String webSavedSsid;
                if (wifiConfigManager.consumeWebSaveSuccess(webSavedSsid))
                {
                    ui_set_config_status("SAVE SUCCESS - BACK TO MENU", 0x00E676);

                    String instruction = "Saved WiFi: ";
                    instruction += webSavedSsid;
                    instruction += ". Returning...";
                    ui_set_config_instruction(instruction.c_str());

                    Serial.printf("[WIFI] Web config saved successfully: %s\n", webSavedSsid.c_str());
                    configAutoBackAt = millis() + 1200;
                }

                if (configAutoBackAt != 0)
                {
                    if (millis() >= configAutoBackAt)
                    {
                        ui_switch_screen(SCR_MENU);
                        configAutoBackAt = 0;
                    }
                    continue;
                }

                if (millis() - lastConfigUiUpdate >= CONFIG_UI_UPDATE_MS)
                {
                    lastConfigUiUpdate = millis();
                    refresh_config_ui(wifiConfigManager);
                }

                publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
                continue;
            }

            if (current_screen_type == SCR_COLLECTDATA)
            {
                if (collectPendingSend)
                {
                    if (ui_consume_collect_take_request())
                    {
                        // Kiểm tra WiFi TRƯỚC, báo lỗi ngay nếu không có mạng
                        if (WiFi.status() != WL_CONNECTED)
                        {
                            char noWifiMsg[96];
                            snprintf(noWifiMsg, sizeof(noWifiMsg),
                                     "Dist:%.0fmm Obj:%.2fC\nKhong co mang WiFi!",
                                     collectPendingDist, collectPendingObj);
                            ui_datacollector_show_popup("LOI MANG", noWifiMsg,
                                                        lv_palette_main(LV_PALETTE_RED), false);
                            lv_timer_handler(); // render ngay
                            delay(1500);
                            ui_datacollector_hide_popup();
                            lv_timer_handler();
                            // KHÔNG reset collectPendingSend → cho phép thử lại khi có mạng
                            continue;
                        }

                        // Có mạng → hiện popup gửi với data
                        char sendingMsg[96];
                        snprintf(sendingMsg, sizeof(sendingMsg),
                                 "Dist:%.0fmm Obj:%.2fC Amb:%.2fC",
                                 collectPendingDist, collectPendingObj, collectPendingAmb);
                        ui_datacollector_show_popup("DANG GUI...", sendingMsg,
                                                    lv_palette_main(LV_PALETTE_BLUE), false);
                        lv_timer_handler(); // render ngay trước khi block HTTP

                        bool sent = false;
                        {
                            WiFiClientSecure secure;
                            secure.setInsecure();
                            HTTPClient http;
                            const String personText = collect_person_id_text(collectPendingPerson);
                            const String sessionText = collect_session_id(collectPendingPerson, collectPendingSession);

                            String url = GOOGLE_SCRIPT_URL +
                                         "?id=" + String(collectPendingPerson) +
                                         "&person=" + personText +
                                         "&session=" + sessionText +
                                         "&session_index=" + String(collectPendingSession) +
                                         "&trial=" + String(collectPendingTrial) +
                                         "&phase=" + String(collect_trial_phase(collectPendingTrial)) +
                                         "&obj=" + String(collectPendingObj, 2) +
                                         "&amb=" + String(collectPendingAmb, 2) +
                                         "&dist=" + String(collectPendingDist, 1) +
                                         "&distance_band=" + String(collect_distance_band(collectPendingDist)) +
                                         "&omron=" +
                                         "&site=axillary" +
                                         "&position=forehead_center" +
                                         "&quality=A" +
                                         "&note=normal" +
                                         "&uptime=" + String(millis() / 1000) +
                                         "&protocol=KLTN_MLX_DCI_AX_V1";

                            if (http.begin(secure, url))
                            {
                                http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // không hỏi lại redirect
                                http.setConnectTimeout(3000);
                                http.setTimeout(4000); // đọc response tối đa 4s
                                int httpCode = http.GET();
                                sent = (httpCode > 0);
                                if (!sent)
                                {
                                    Serial.printf("[COLLECT] HTTP error: %s\n",
                                                  http.errorToString(httpCode).c_str());
                                }
                                http.end();
                            }
                            else
                            {
                                Serial.println("[COLLECT] HTTP begin failed");
                            }
                        }

                        if (sent)
                        {
                            collectSampleCount++;
                            ui_datacollector_update_progress(collectSampleCount, COLLECT_SAMPLE_LIMIT);
                            char okMsg[96];
                            snprintf(okMsg, sizeof(okMsg),
                                     "Dist:%.0fmm Obj:%.2fC\nMau %d/%d da gui!",
                                     collectPendingDist, collectPendingObj,
                                     collectSampleCount, COLLECT_SAMPLE_LIMIT);
                            ui_datacollector_show_popup("THANH CONG", okMsg,
                                                        lv_palette_main(LV_PALETTE_GREEN), false);
                            collectPendingSend = false;
                        }
                        else
                        {
                            char failMsg[96];
                            snprintf(failMsg, sizeof(failMsg),
                                     "Dist:%.0fmm Obj:%.2fC\nLoi cloud, thu lai?",
                                     collectPendingDist, collectPendingObj);
                            ui_datacollector_show_popup("GUI THAT BAI", failMsg,
                                                        lv_palette_main(LV_PALETTE_RED), false);
                            // KHÔNG reset collectPendingSend → bấm ENTER thử lại
                        }

                        lv_timer_handler(); // render popup kết quả ngay
                        delay(250);         // chỉ đủ để người đọc thấy, không block lâu
                        lv_timer_handler();
                        ui_datacollector_hide_popup();
                        lv_timer_handler();
                    }
                    else if (ui_consume_collect_reset_request())
                    {
                        ui_datacollector_hide_popup();
                        collectPendingSend = false;
                    }

                    continue;
                }

                if (ui_consume_collect_id_minus_request())
                {
                    if (collectCurrentId > 1)
                    {
                        collectCurrentId--;
                    }
                    else
                    {
                        collectCurrentId = COLLECT_PERSON_MAX;
                    }

                    collectSampleCount = 0;
                    collect_refresh_subject_ui();
                }

                if (ui_consume_collect_id_plus_request())
                {
                    if (collectCurrentId < COLLECT_PERSON_MAX)
                    {
                        collectCurrentId++;
                    }
                    else
                    {
                        collectCurrentId = 1;
                    }

                    collectSampleCount = 0;
                    collect_refresh_subject_ui();
                }

                if (ui_consume_collect_session_plus_request())
                {
                    if (collectCurrentSession < COLLECT_SESSION_MAX)
                    {
                        collectCurrentSession++;
                    }
                    else
                    {
                        collectCurrentSession = 1;
                    }

                    collectSampleCount = 0;
                    collect_refresh_subject_ui();
                }

                if (ui_consume_collect_reset_request())
                {
                    collectSampleCount = 0;
                    collect_refresh_subject_ui();
                }

                if (ui_consume_collect_take_request())
                {
                    collect_perform_measurement(wifiConfigManager);
                }

                if (millis() - collectLastLiveUpdate >= 120)
                {
                    collectLastLiveUpdate = millis();
                    collectLastDistance = collect_live_distance();

                    float displayDistance = collectLastDistance;

                    float liveTemp = sensorRuntime.mlxReady() ? sensorRuntime.mlxBodyTempC() : -1.0f;
                    float liveAmb = sensorRuntime.mlxConnected() ? sensorRuntime.mlxAmbientTempC() : -999.0f;
                    ui_datacollector_update_sensors(displayDistance, liveTemp, liveAmb);
                }

                continue;
            }

            if (current_screen_type == SCR_MEASUREALL)
            {
                float liveTemp = sensorRuntime.mlxReady() ? sensorRuntime.mlxBodyTempC() : -1.0f;
                SensorSnapshot maxSnap = sensorRuntime.maxSnapshot();
                SensorSnapshot ecgSnap = sensorRuntime.ecgSnapshot();
                int hr = maxSnap.signalReady ? maxSnap.heartRateBpm : 0;
                int spo2 = maxSnap.signalReady ? maxSnap.spo2Percent : 0;
                bool ecgLive = sensorRuntime.ad8232Ready() && ecgSnap.sensorReady && ecgSnap.signalReady;
                float ecgVal = ecgLive ? getECGFilteredSignal() : 0.0f;
                collectLastDistance = collect_live_distance();

                float displayDist = collectLastDistance;

                // Cập nhật latestEcgLive/latestEcgSample để publishTelemetryIfReady dùng
                latestEcgLive = ecgLive;
                latestEcgSample = ecgVal;
                if (maxSnap.signalReady)
                {
                    latestMaxLive = true;
                    latestMaxSnapshot = maxSnap;
                }
                else
                {
                    latestMaxLive = false;
                }

                ui_set_measure_all_values(liveTemp, hr, spo2, ecgVal, displayDist);
                publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
                continue;
            }

            if (!is_max30102_screen(current_screen_type))
            {
                latestMaxLive = false;
                if (!is_ecg_screen(current_screen_type))
                {
                    latestEcgLive = false;
                    if (is_temp_screen(current_screen_type))
                    {
                        const float tempForUi = sensorRuntime.mlxReady() ? sensorRuntime.mlxBodyTempC() : -1.0f;
                        ui_update_sensors(tempForUi, 0, 0, 50);
                        ui_set_ambient_temp(sensorRuntime.mlxConnected() ? sensorRuntime.mlxAmbientTempC() : -999.0f);

                        static unsigned long lastTempDistUpdate = 0;
                        if (millis() - lastTempDistUpdate >= 200)
                        {
                            lastTempDistUpdate = millis();
                            float distMm = 999.0f;
                            if (collect_ensure_lox_ready())
                            {
                                distMm = collect_live_distance();
                            }
                            ui_set_temp_distance(distMm);
                        }

                        if (sensorRuntime.mlxReady())
                        {
                            ui_set_sensor_status("MLX90614 LIVE", 0x00E676);
                        }
                        else if (sensorRuntime.mlxConnected())
                        {
                            ui_set_sensor_status("MLX90614 DANG DO...", 0xFFB300);
                        }
                        else
                        {
                            ui_set_sensor_status("NO MLX90614 - CHECK WIRING", 0xFF5252);
                        }

                        if (millis() - lastTempDiagLog >= LIVE_LOG_INTERVAL_MS)
                        {
                            lastTempDiagLog = millis();
                            Serial.printf("[TEMP] connected=%d ready=%d body=%.2f ambient=%.2f\n",
                                          sensorRuntime.mlxConnected() ? 1 : 0,
                                          sensorRuntime.mlxReady() ? 1 : 0,
                                          sensorRuntime.mlxBodyTempC(),
                                          sensorRuntime.mlxAmbientTempC());
                        }
                    }

                    publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
                    continue;
                }

                const SensorSnapshot e = sensorRuntime.ecgSnapshot();

                EcgRuntimeState rawEcgState;
                if (!sensorRuntime.ad8232Ready() || !e.sensorReady)
                {
                    rawEcgState = EcgRuntimeState::NoSensor;
                }
                else if (!e.signalReady)
                {
                    rawEcgState = EcgRuntimeState::LeadsOff;
                }
                else
                {
                    rawEcgState = EcgRuntimeState::LiveData;
                }

                if (update_stable_state(ecgStateTracker, rawEcgState))
                {
                    EcgRuntimeState ecgState = ecgStateTracker.stable;
                    if (ecgState == EcgRuntimeState::NoSensor)
                    {
                        Serial.println("[ECG] AD8232/ADS1115 not detected. ECG chart in placeholder mode.");
                    }
                    else if (ecgState == EcgRuntimeState::LeadsOff)
                    {
                        Serial.println("[ECG] Sensor ready. Waiting for ECG leads.");
                    }
                    else
                    {
                        Serial.println("[ECG] Live signal detected.");
                    }

                    notify_ecg_state(ecgState);
                }

                if (rawEcgState != EcgRuntimeState::LiveData)
                {
                    latestEcgLive = false;
                }

                publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
                continue;
            }

            if (!sensorRuntime.maxReady())
            {
                const float tempForUi = (current_screen_type == SCR_MONITOR && sensorRuntime.mlxReady()) ? sensorRuntime.mlxBodyTempC() : -1.0f;
                ui_update_sensors(tempForUi, 0, 0, 50);
                if (current_screen_type == SCR_MONITOR)
                {
                    ui_set_ambient_temp(sensorRuntime.mlxReady() ? sensorRuntime.mlxAmbientTempC() : -999.0f);
                }

                publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
                continue;
            }

            const SensorSnapshot s = sensorRuntime.maxSnapshot();

            MaxRuntimeState rawMaxState;
            if (!s.sensorReady)
            {
                rawMaxState = MaxRuntimeState::NoSensor;
            }
            else if (!s.signalReady)
            {
                rawMaxState = MaxRuntimeState::WaitingFinger;
            }
            else
            {
                rawMaxState = MaxRuntimeState::LiveData;
            }

            const float tempForUi = (current_screen_type == SCR_MONITOR && sensorRuntime.mlxReady()) ? sensorRuntime.mlxBodyTempC() : -1.0f;
            if (rawMaxState == MaxRuntimeState::LiveData)
            {
                latestMaxLive = true;
                latestMaxSnapshot = s;
                ui_update_sensors(tempForUi, s.heartRateBpm, s.spo2Percent, s.waveform);
            }
            else
            {
                latestMaxLive = false;
                ui_update_sensors(tempForUi, 0, 0, 50);
            }

            if (current_screen_type == SCR_MONITOR)
            {
                ui_set_ambient_temp(sensorRuntime.mlxReady() ? sensorRuntime.mlxAmbientTempC() : -999.0f);
            }

            if (update_stable_state(maxStateTracker, rawMaxState))
            {
                MaxRuntimeState maxState = maxStateTracker.stable;
                if (maxState == MaxRuntimeState::NoSensor)
                {
                    Serial.println("[MAX30102] No sensor detected. UI running in placeholder mode.");
                }
                else if (maxState == MaxRuntimeState::WaitingFinger)
                {
                    Serial.println("[MAX30102] Sensor ready. Waiting for finger placement.");
                }
                else
                {
                    Serial.print("[MAX30102] Live HR=");
                    Serial.print(s.heartRateBpm);
                    Serial.print(" bpm, SpO2=");
                    Serial.print(s.spo2Percent);
                    Serial.print("%, Wave=");
                    Serial.println(s.waveform);
                    lastMaxLiveLog = millis();
                }

                notify_max_state(maxState,
                                 current_screen_type == SCR_MONITOR,
                                 sensorRuntime.maxReady(),
                                 sensorRuntime.mlxReady());
            }
            else if (maxStateTracker.stable == MaxRuntimeState::LiveData && (millis() - lastMaxLiveLog >= LIVE_LOG_INTERVAL_MS))
            {
                Serial.print("[MAX30102] Live HR=");
                Serial.print(s.heartRateBpm);
                Serial.print(" bpm, SpO2=");
                Serial.print(s.spo2Percent);
                Serial.print("%, Wave=");
                Serial.println(s.waveform);
                lastMaxLiveLog = millis();
            }

            publishTelemetryIfReady(wifiConfigManager, sensorRuntime, current_screen_type);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    pinMode(TP5100_CHRG_PIN, INPUT);
    pinMode(TP5100_FULL_PIN, INPUT);

    Serial.printf("[BOOT] reset_reason=%d\n", esp_reset_reason());
    Serial.printf("[BOOT] chip_rev=%d\n", ESP.getChipRevision());

    // KHỞI TẠO I2C VỚI XUNG NHỊP 50kHz CHO DÂY 1 MÉT
    Wire.begin(21, 22);
    Wire.setClock(50000);

    // ==========================================
    // I2C SCANNER ĐỂ CHẨN ĐOÁN
    // ==========================================
    Serial.println("\n[I2C SCANNER] Bắt đầu quét I2C bus 0 (SDA: 21, SCL: 22)...");
    byte error, address;
    int nDevices = 0;

    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("[I2C SCANNER] Đã tìm thấy thiết bị tại địa chỉ 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("[I2C SCANNER] Lỗi không xác định tại địa chỉ 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }

    if (nDevices == 0)
    {
        Serial.println("[I2C SCANNER] KHÔNG TÌM THẤY THIẾT BỊ NÀO!");
    }
    else
    {
        Serial.println("[I2C SCANNER] Quét xong.\n");
    }
    // ==========================================

    Wire1.begin(4, 5);
    Wire1.setClock(50000);
    // ==========================================
    // I2C SCANNER ĐỂ CHẨN ĐOÁN
    // ==========================================
    Serial.println("\n[I2C SCANNER] Bắt đầu quét I2C bus 1 (SDA: 4, SCL: 5)...");

    for (address = 1; address < 127; address++)
    {
        Wire1.beginTransmission(address);
        error = Wire1.endTransmission();

        if (error == 0)
        {
            Serial.print("[I2C SCANNER] Đã tìm thấy thiết bị tại địa chỉ 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("[I2C SCANNER] Lỗi không xác định tại địa chỉ 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }

    if (nDevices == 0)
    {
        Serial.println("[I2C SCANNER] KHÔNG TÌM THẤY THIẾT BỊ NÀO!");
    }
    else
    {
        Serial.println("[I2C SCANNER] Quét xong.\n");
    }
    // ==========================================

    // Tạo Mutex cho Bus 0
    i2c0Mutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(guiTask, "LVGL", 32768, NULL, 1, NULL, 1);
}

void loop() { delay(1000); }