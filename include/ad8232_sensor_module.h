#ifndef AD8232_SENSOR_MODULE_H
#define AD8232_SENSOR_MODULE_H

#include <Arduino.h>
#include "sensor_module.h"

class Ad8232SensorModule : public SensorModule
{
public:
    Ad8232SensorModule();

    bool begin() override;
    void update() override;
    SensorSnapshot snapshot() const override;

private:
    static constexpr uint32_t kSampleIntervalUs = 4000; // 250Hz like Main_Ad8232
    static constexpr uint8_t kMaxCatchupSamplesPerUpdate = 16;

    bool initialized_;
    SensorSnapshot currentSnapshot_;
    float displayBaseline_;
    float displayFiltered_;
    float envelope_;
    float yScale_;
    bool needsReprime_;
    uint32_t lastSampleAtUs_;

    int mapFilteredSignalToChart(float filteredSignalMv);
};

#endif