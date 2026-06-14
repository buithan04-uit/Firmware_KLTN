#ifndef MLX90614_SENSOR_MODULE_H
#define MLX90614_SENSOR_MODULE_H

#include <Arduino.h>
#include <Adafruit_MLX90614.h>
#include "sensor_module.h"

class Mlx90614SensorModule : public SensorModule
{
public:
    Mlx90614SensorModule(uint8_t sdaPin = 21, uint8_t sclPin = 22);

    bool begin() override;
    void update() override;
    SensorSnapshot snapshot() const override;

    float ambientTempC() const;

    // Called by the main loop with the latest VL53L0X distance (mm) between
    // the sensor and the target so compensateBodyTemp() can correct for the
    // IR spot growing/cooling with distance. Pass <=0 or >=999 for "unknown".
    void setTargetDistanceMm(float distanceMm);

private:
    static constexpr uint32_t kReadIntervalMs = 1500;
    static constexpr uint32_t kRetryIntervalMs = 2500;
    static constexpr float kFilterAlpha = 0.25f;
    static constexpr uint32_t kI2CFrequencyHz = 100000;
    static constexpr uint8_t kReadFailThreshold = 5;
    // 4-sample moving average @ ~1.5s/read = ~6s window: smooths sensor
    // noise while still tracking real temperature changes within a few
    // seconds (a 10-sample/15s window felt too sluggish to the user).
    static constexpr uint8_t kObjectFilterSize = 4;
    static constexpr float kBodyThresholdC = 32.0f;
    static constexpr float kBodyUpperBoundC = 42.0f;

    uint8_t sdaPin_;
    uint8_t sclPin_;

    Adafruit_MLX90614 sensor_;
    SensorSnapshot currentSnapshot_;

    bool ready_;
    uint8_t readErrorCount_;
    uint32_t lastReadAtMs_;
    uint32_t lastRetryAtMs_;

    float ambientTempC_;
    float objectFilterBuffer_[kObjectFilterSize];
    float objectFilterSum_;
    uint8_t objectFilterIndex_;
    uint8_t objectFilterCount_;
    float targetDistanceMm_;

    void ensureI2cConfigured();
    float pushAndGetFilteredObjectTemp(float rawObjectTempC);
    float compensateBodyTemp(float objectTempC, float ambientTempC) const;
    static bool isValidSample(float objectTempC, float ambientTempC);
};

#endif
