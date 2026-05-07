#include "mlx90614_sensor_module.h"

#include <Wire.h>
#include <math.h>

Mlx90614SensorModule::Mlx90614SensorModule(uint8_t sdaPin, uint8_t sclPin)
    : sdaPin_(sdaPin),
      sclPin_(sclPin),
      ready_(false),
      readErrorCount_(0),
      lastReadAtMs_(0),
      lastRetryAtMs_(0),
      ambientTempC_(-999.0f),
      objectFilterSum_(0.0f),
      objectFilterIndex_(0),
      objectFilterCount_(0)
{
    for (uint8_t i = 0; i < kObjectFilterSize; ++i)
    {
        objectFilterBuffer_[i] = 0.0f;
    }

    currentSnapshot_.bodyTempC = -1.0f;
    currentSnapshot_.sensorReady = false;
    currentSnapshot_.signalReady = false;
}

bool Mlx90614SensorModule::begin()
{
    ensureI2cConfigured();

    ready_ = sensor_.begin();
    readErrorCount_ = 0;
    lastReadAtMs_ = 0;
    lastRetryAtMs_ = millis();
    objectFilterSum_ = 0.0f;
    objectFilterIndex_ = 0;
    objectFilterCount_ = 0;

    for (uint8_t i = 0; i < kObjectFilterSize; ++i)
    {
        objectFilterBuffer_[i] = 0.0f;
    }

    currentSnapshot_.sensorReady = ready_;
    // Require at least one valid sample before exposing LIVE data.
    currentSnapshot_.signalReady = false;

    if (ready_)
    {
        Serial.println("[MLX90614] Sensor initialized.");
    }
    else
    {
        Serial.println("[MLX90614] Sensor not found, background retry enabled.");
    }

    return ready_;
}

void Mlx90614SensorModule::update()
{
    const uint32_t nowMs = millis();
    static uint32_t lastDiagLogAtMs = 0;

    if (!ready_)
    {
        if (nowMs - lastRetryAtMs_ < kRetryIntervalMs)
        {
            return;
        }

        lastRetryAtMs_ = nowMs;
        ensureI2cConfigured();
        ready_ = sensor_.begin();
        currentSnapshot_.sensorReady = ready_;
        currentSnapshot_.signalReady = false;

        if (ready_)
        {
            readErrorCount_ = 0;
            objectFilterSum_ = 0.0f;
            objectFilterIndex_ = 0;
            objectFilterCount_ = 0;
            for (uint8_t i = 0; i < kObjectFilterSize; ++i)
            {
                objectFilterBuffer_[i] = 0.0f;
            }
            Serial.println("[MLX90614] Sensor reconnected.");
        }
        return;
    }

    if (nowMs - lastReadAtMs_ < kReadIntervalMs)
    {
        return;
    }
    lastReadAtMs_ = nowMs;

    // MAX30102/other modules may change I2C clock; force MLX-safe config before each read.
    ensureI2cConfigured();

    const float objectTempC = sensor_.readObjectTempC();
    delay(2);
    const float ambientTempC = sensor_.readAmbientTempC();
    if (!isValidSample(objectTempC, ambientTempC))
    {
        readErrorCount_++;
        currentSnapshot_.signalReady = false;

        if (readErrorCount_ == 1 || readErrorCount_ == kReadFailThreshold)
        {
            Serial.printf("[MLX90614] Invalid sample obj=%.2f amb=%.2f (count=%u)\n",
                          objectTempC,
                          ambientTempC,
                          static_cast<unsigned>(readErrorCount_));
        }

        if (readErrorCount_ >= kReadFailThreshold)
        {
            ready_ = false;
            readErrorCount_ = 0;
            lastRetryAtMs_ = nowMs;
            currentSnapshot_.sensorReady = false;
            currentSnapshot_.signalReady = false;
            currentSnapshot_.bodyTempC = -1.0f;
            ambientTempC_ = -999.0f;
            Serial.println("[MLX90614] Read failed repeatedly, waiting for reconnect.");
        }
        return;
    }

    readErrorCount_ = 0;

    // Raw mode for LCD: expose direct MLX object/ambient values with no smoothing
    currentSnapshot_.bodyTempC = objectTempC;
    ambientTempC_ = ambientTempC;

    currentSnapshot_.sensorReady = true;
    currentSnapshot_.signalReady = true;

    if (nowMs - lastDiagLogAtMs >= 1500)
    {
        lastDiagLogAtMs = nowMs;
        Serial.printf("[MLX90614] RAW obj=%.2f amb=%.2f\n",
                      currentSnapshot_.bodyTempC,
                      ambientTempC_);
    }
}

SensorSnapshot Mlx90614SensorModule::snapshot() const
{
    return currentSnapshot_;
}

float Mlx90614SensorModule::ambientTempC() const
{
    return ambientTempC_;
}

void Mlx90614SensorModule::ensureI2cConfigured()
{
    Wire.setClock(kI2CFrequencyHz);
}

float Mlx90614SensorModule::pushAndGetFilteredObjectTemp(float rawObjectTempC)
{
    if (objectFilterCount_ < kObjectFilterSize)
    {
        objectFilterCount_++;
    }
    else
    {
        objectFilterSum_ -= objectFilterBuffer_[objectFilterIndex_];
    }

    objectFilterBuffer_[objectFilterIndex_] = rawObjectTempC;
    objectFilterSum_ += rawObjectTempC;
    objectFilterIndex_ = (objectFilterIndex_ + 1) % kObjectFilterSize;

    if (objectFilterCount_ == 0)
    {
        return rawObjectTempC;
    }

    return objectFilterSum_ / static_cast<float>(objectFilterCount_);
}

float Mlx90614SensorModule::compensateBodyTemp(float objectTempC, float ambientTempC)
{
    float bodyTemp = objectTempC;

    if (ambientTempC < 25.0f)
    {
        bodyTemp += (25.0f - ambientTempC) * 0.1f;
    }

    if (bodyTemp < 32.0f)
        return bodyTemp;
    if (bodyTemp < 34.0f)
        return bodyTemp + 3.0f;
    if (bodyTemp < 35.5f)
        return bodyTemp + 2.4f;
    if (bodyTemp < 38.0f)
        return bodyTemp + 1.4f;
    return bodyTemp + 0.5f;
}

bool Mlx90614SensorModule::isValidSample(float objectTempC, float ambientTempC)
{
    if (isnan(objectTempC) || isnan(ambientTempC))
    {
        return false;
    }

    if (objectTempC < -20.0f || objectTempC > 120.0f)
    {
        return false;
    }

    if (ambientTempC < -20.0f || ambientTempC > 80.0f)
    {
        return false;
    }

    return true;
}
