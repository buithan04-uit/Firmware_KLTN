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
      objectFilterCount_(0),
      targetDistanceMm_(-1.0f)
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

    // Smooth the raw object temperature (10-sample moving average @ ~1.5s
    // interval = ~15s window) before calibration, since IR readings are noisy
    // sample-to-sample but body temperature changes slowly.
    const float filteredObjectTempC = pushAndGetFilteredObjectTemp(objectTempC);
    currentSnapshot_.bodyTempC = compensateBodyTemp(filteredObjectTempC, ambientTempC);
    ambientTempC_ = ambientTempC;

    currentSnapshot_.sensorReady = true;
    currentSnapshot_.signalReady = true;

    if (nowMs - lastDiagLogAtMs >= 1500)
    {
        lastDiagLogAtMs = nowMs;
        Serial.printf("[MLX90614] obj_raw=%.2f obj_filt=%.2f amb=%.2f body_cal=%.2f\n",
                      objectTempC,
                      filteredObjectTempC,
                      ambientTempC_,
                      currentSnapshot_.bodyTempC);
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

void Mlx90614SensorModule::setTargetDistanceMm(float distanceMm)
{
    targetDistanceMm_ = distanceMm;
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

float Mlx90614SensorModule::compensateBodyTemp(float objectTempC, float ambientTempC) const
{
    // Ambient compensation: in a cold room more heat radiates away between the
    // skin and the sensor, so the raw reading under-reports actual skin temp.
    float skinTempC = objectTempC;
    if (ambientTempC < 25.0f)
    {
        skinTempC += (25.0f - ambientTempC) * 0.1f;
    }

    // Distance compensation: the MLX90614's ~35 deg FOV spot grows with
    // distance, increasingly mixing in cooler surrounding air/background
    // instead of pure skin, so the raw reading drops as the sensor moves
    // away from the forehead. The calibration curve below was characterized
    // at a reference distance of ~30mm, so add back the estimated loss for
    // larger distances (clamped to a plausible range).
    if (targetDistanceMm_ > 0.0f && targetDistanceMm_ < 200.0f)
    {
        constexpr float kRefDistanceMm = 30.0f;
        constexpr float kDistanceCoeffCPerMm = 0.02f; // ~0.2 C per extra cm
        const float extraDistanceMm = targetDistanceMm_ - kRefDistanceMm;
        if (extraDistanceMm > 0.0f)
        {
            skinTempC += constrain(extraDistanceMm * kDistanceCoeffCPerMm, 0.0f, 2.5f);
        }
    }

    // Forehead/skin IR temperature reads below oral/core temperature, and the
    // gap narrows as temperature rises (vasodilation brings more blood flow to
    // the skin during fever). Map skin temp -> oral-equivalent body temp with a
    // piecewise-linear calibration curve so the output stays continuous (no
    // step jumps that would destabilise the downstream AI vitals model, which
    // is trained on oral-equivalent values centered around 36.75 C).
    struct CalPoint
    {
        float skin;
        float oral;
    };
    static const CalPoint kCalCurve[] = {
        {28.0f, 32.0f},
        {32.0f, 35.3f},
        {34.0f, 36.4f},
        {35.5f, 37.0f},
        {37.0f, 38.2f},
        {39.0f, 39.8f},
        {42.0f, 42.5f},
    };
    constexpr int kCalCount = sizeof(kCalCurve) / sizeof(kCalCurve[0]);

    if (skinTempC <= kCalCurve[0].skin)
    {
        const float slope = (kCalCurve[1].oral - kCalCurve[0].oral) / (kCalCurve[1].skin - kCalCurve[0].skin);
        return kCalCurve[0].oral + (skinTempC - kCalCurve[0].skin) * slope;
    }
    if (skinTempC >= kCalCurve[kCalCount - 1].skin)
    {
        const float slope = (kCalCurve[kCalCount - 1].oral - kCalCurve[kCalCount - 2].oral) /
                             (kCalCurve[kCalCount - 1].skin - kCalCurve[kCalCount - 2].skin);
        return kCalCurve[kCalCount - 1].oral + (skinTempC - kCalCurve[kCalCount - 1].skin) * slope;
    }

    for (int i = 0; i < kCalCount - 1; i++)
    {
        if (skinTempC >= kCalCurve[i].skin && skinTempC <= kCalCurve[i + 1].skin)
        {
            const float t = (skinTempC - kCalCurve[i].skin) / (kCalCurve[i + 1].skin - kCalCurve[i].skin);
            return kCalCurve[i].oral + t * (kCalCurve[i + 1].oral - kCalCurve[i].oral);
        }
    }

    return skinTempC; // unreachable
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
