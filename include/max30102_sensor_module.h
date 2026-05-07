#ifndef MAX30102_SENSOR_MODULE_H
#define MAX30102_SENSOR_MODULE_H

#include <Arduino.h>
#include "MAX30105.h"
#include "sensor_module.h"

class Max30102SensorModule : public SensorModule
{
public:
    Max30102SensorModule(uint8_t sdaPin = 21, uint8_t sclPin = 22);

    bool begin() override;
    void update() override;
    SensorSnapshot snapshot() const override;

private:
    static const int kBufferSize = 100;
    static const int kSamplesToRead = 25;
    static const uint32_t kFingerThreshold = 15000;

    uint8_t sdaPin_;
    uint8_t sclPin_;

    MAX30105 sensor_;
    SensorSnapshot currentSnapshot_;

    bool ready_;
    bool fingerPresent_;

    uint32_t irBuffer_[kBufferSize];
    uint32_t redBuffer_[kBufferSize];
    int bufferCount_;
    int samplesSinceCalc_;

    int32_t maximSpo2_;
    int8_t validSpo2_;
    int32_t maximHeartRate_;
    int8_t validHeartRate_;

    double filteredSpo2_;
    double filteredHeartRate_;
    unsigned long lastValidHrTime_;
    float trackedIrMin_;
    float trackedIrMax_;

    void resetMetrics();
    double calculateRmsSpo2(const uint32_t *redBuf, const uint32_t *irBuf, int length) const;
    int toPlethScale(uint32_t irValue);
};

#endif