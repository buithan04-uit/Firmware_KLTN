#include "lcd_sensor_runtime.h"

void LcdSensorRuntime::begin()
{
    if (mlx90614Module_.begin())
    {
        Serial.println("[MLX90614] Initialized for monitor/temp.");
    }
    else
    {
        Serial.println("[MLX90614] Not detected, will retry in background.");
    }

    // Keep ECG path behavior closer to Main_Ad8232: initialize once at startup.
    ad8232Ready_ = beginAd8232();
    if (ad8232Ready_)
    {
        Serial.println("[ECG] AD8232/ADS1115 initialized for ECG screen.");
    }
    else
    {
        Serial.println("[ECG] AD8232/ADS1115 unavailable at startup.");
    }

    // Sync mlxConnected_ immediately from begin() result so boot screen
    // reads correct status without needing updateBackground() first.
    const SensorSnapshot mlxInitSnap = mlx90614Module_.snapshot();
    mlxConnected_ = mlxInitSnap.sensorReady;
    mlxReady_ = mlxInitSnap.sensorReady && mlxInitSnap.signalReady && (mlxInitSnap.bodyTempC > 0.0f);
    if (mlxConnected_)
    {
        mlxBodyTempC_ = mlxInitSnap.bodyTempC;
        mlxAmbientTempC_ = mlx90614Module_.ambientTempC();
    }
}

void LcdSensorRuntime::updateBackground(bool enableMlxUpdate)
{
    if (!enableMlxUpdate)
    {
        return;
    }

    mlx90614Module_.update();

    const SensorSnapshot mlxSnapshot = mlx90614Module_.snapshot();
    mlxConnected_ = mlxSnapshot.sensorReady;
    mlxReady_ = mlxSnapshot.sensorReady && mlxSnapshot.signalReady && (mlxSnapshot.bodyTempC > 0.0f);

    if (mlxReady_)
    {
        mlxBodyTempC_ = mlxSnapshot.bodyTempC;
        mlxAmbientTempC_ = mlx90614Module_.ambientTempC();
    }
    else if (!mlxConnected_)
    {
        mlxBodyTempC_ = -1.0f;
        mlxAmbientTempC_ = -999.0f;
    }
}

void LcdSensorRuntime::updateEcgBackground(bool enableEcgUpdate)
{
    if (!enableEcgUpdate || !ad8232Initialized_)
    {
        return;
    }

    ad8232Module_.update();
    const SensorSnapshot latest = ad8232Module_.snapshot();
    portENTER_CRITICAL(&ecgMux_);
    ecgSnapshot_ = latest;
    ad8232Ready_ = latest.sensorReady;
    portEXIT_CRITICAL(&ecgMux_);
}

bool LcdSensorRuntime::beginMax30102()
{
    maxReady_ = max30102Module_.begin();
    return maxReady_;
}

bool LcdSensorRuntime::beginAd8232()
{
    if (!ad8232Initialized_)
    {
        ad8232Ready_ = ad8232Module_.begin();
        ad8232Initialized_ = true;
        const SensorSnapshot initial = ad8232Module_.snapshot();
        portENTER_CRITICAL(&ecgMux_);
        ecgSnapshot_ = initial;
        portEXIT_CRITICAL(&ecgMux_);
    }
    return ad8232Ready_;
}

SensorSnapshot LcdSensorRuntime::updateMax30102()
{
    if (!maxReady_)
    {
        return SensorSnapshot{};
    }

    max30102Module_.update();

    // Khóa MUX để an toàn ghi data
    portENTER_CRITICAL(&maxMux_);
    maxSnapshot_ = max30102Module_.snapshot();
    portEXIT_CRITICAL(&maxMux_);

    return maxSnapshot_;
}

SensorSnapshot LcdSensorRuntime::updateAd8232()
{
    // Legacy API kept for compatibility; ECG is now sampled in updateEcgBackground().
    return ecgSnapshot_;
}

SensorSnapshot LcdSensorRuntime::ecgSnapshot() const
{
    portENTER_CRITICAL(&ecgMux_);
    SensorSnapshot copy = ecgSnapshot_;
    portEXIT_CRITICAL(&ecgMux_);
    return copy;
}

bool LcdSensorRuntime::maxReady() const
{
    return maxReady_;
}

bool LcdSensorRuntime::ad8232Ready() const
{
    portENTER_CRITICAL(&ecgMux_);
    bool ready = ad8232Ready_;
    portEXIT_CRITICAL(&ecgMux_);
    return ready;
}

bool LcdSensorRuntime::mlxReady() const
{
    return mlxReady_;
}

bool LcdSensorRuntime::mlxConnected() const
{
    return mlxConnected_;
}

float LcdSensorRuntime::mlxBodyTempC() const
{
    return mlxBodyTempC_;
}

float LcdSensorRuntime::mlxAmbientTempC() const
{
    return mlxAmbientTempC_;
}

SensorSnapshot LcdSensorRuntime::maxSnapshot() const
{
    portENTER_CRITICAL(&maxMux_);
    SensorSnapshot copy = maxSnapshot_;
    portEXIT_CRITICAL(&maxMux_);
    return copy;
}