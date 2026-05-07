#ifndef LCD_SENSOR_RUNTIME_H
#define LCD_SENSOR_RUNTIME_H

#include <Arduino.h>
#include "sensor_module.h"
#include "max30102_sensor_module.h"
#include "mlx90614_sensor_module.h"
#include "ad8232_sensor_module.h"

class LcdSensorRuntime
{
public:
    void begin();
    void updateBackground(bool enableMlxUpdate);
    void updateEcgBackground(bool enableEcgUpdate);

    bool beginMax30102();
    bool beginAd8232();

    SensorSnapshot updateMax30102();
    SensorSnapshot updateAd8232();
    SensorSnapshot ecgSnapshot() const;
    SensorSnapshot maxSnapshot() const;

    bool maxReady() const;
    bool ad8232Ready() const;
    bool mlxReady() const;
    bool mlxConnected() const;

    float mlxBodyTempC() const;
    float mlxAmbientTempC() const;

private:
    Max30102SensorModule max30102Module_;
    Mlx90614SensorModule mlx90614Module_;
    Ad8232SensorModule ad8232Module_;

    bool maxReady_ = false;
    bool ad8232Initialized_ = false;
    bool ad8232Ready_ = false;
    bool mlxReady_ = false;
    bool mlxConnected_ = false;

    float mlxBodyTempC_ = -1.0f;
    float mlxAmbientTempC_ = -999.0f;

    SensorSnapshot maxSnapshot_;
    SensorSnapshot ecgSnapshot_;
    mutable portMUX_TYPE ecgMux_ = portMUX_INITIALIZER_UNLOCKED;
    mutable portMUX_TYPE maxMux_ = portMUX_INITIALIZER_UNLOCKED;
};

#endif
