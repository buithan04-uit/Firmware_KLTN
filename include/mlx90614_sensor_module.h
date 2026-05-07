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

private:
    static constexpr uint32_t kReadIntervalMs = 1500;
    static constexpr uint32_t kRetryIntervalMs = 2500;
    static constexpr float kFilterAlpha = 0.25f;
    static constexpr uint32_t kI2CFrequencyHz = 100000;
    static constexpr uint8_t kReadFailThreshold = 5;
    static constexpr uint8_t kObjectFilterSize = 10;
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
    float pushAndGetFilteredObjectTemp(float rawObjectTempC);
    static float compensateBodyTemp(float objectTempC, float ambientTempC);
    static bool isValidSample(float objectTempC, float ambientTempC);
};

#endif
