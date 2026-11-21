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

    // NOTE: VL53L0X distance is intentionally NOT fed into this module.
    // The DCI variant's 5deg FOV keeps the IR spot on the forehead at normal
    // measuring range; distance is only shown on the UI so the user can hold
    // the device at 3-4cm. Feeding it into the temperature math previously
    // fabricated +2.5C spikes when pulling the device away.

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

    void ensureI2cConfigured();
    void resetObjectFilter();
    float pushAndGetFilteredObjectTemp(float rawObjectTempC);
    float compensateBodyTemp(float objectTempC, float ambientTempC) const;
    static bool isValidSample(float objectTempC, float ambientTempC);
};

#endif
