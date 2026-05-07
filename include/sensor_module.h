#ifndef SENSOR_MODULE_H
#define SENSOR_MODULE_H

#include <Arduino.h>

struct SensorSnapshot
{
    float bodyTempC = -1.0f;
    int heartRateBpm = 0;
    int spo2Percent = 0;
    int waveform = 50;
    bool sensorReady = false;
    bool signalReady = false;
};

class SensorModule
{
public:
    virtual ~SensorModule() = default;
    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual SensorSnapshot snapshot() const = 0;
};

#endif