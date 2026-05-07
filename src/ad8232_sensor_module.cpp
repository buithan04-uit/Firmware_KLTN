#include "ad8232_sensor_module.h"

#include "ad8232.h"
#include <math.h>

Ad8232SensorModule::Ad8232SensorModule()
    : initialized_(false),
      displayBaseline_(0.0f),
      displayFiltered_(0.0f),
      envelope_(130.0f),
      yScale_(230.0f),
      needsReprime_(true),
      lastSampleAtUs_(0)
{
    currentSnapshot_.sensorReady = false;
    currentSnapshot_.signalReady = false;
    currentSnapshot_.waveform = 50;
}

bool Ad8232SensorModule::begin()
{
    initAD8232();
    initialized_ = true;

    currentSnapshot_.sensorReady = isAD8232Available();
    currentSnapshot_.signalReady = false;
    currentSnapshot_.heartRateBpm = 0;
    currentSnapshot_.spo2Percent = 0;
    currentSnapshot_.bodyTempC = -1.0f;
    currentSnapshot_.waveform = 50;

    displayBaseline_ = 0.0f;
    displayFiltered_ = 0.0f;
    envelope_ = 130.0f;
    yScale_ = 230.0f;
    needsReprime_ = true;
    lastSampleAtUs_ = micros();

    return currentSnapshot_.sensorReady;
}

void Ad8232SensorModule::update()
{
    if (!initialized_)
    {
        return;
    }

    currentSnapshot_.sensorReady = isAD8232Available();
    if (!currentSnapshot_.sensorReady)
    {
        currentSnapshot_.signalReady = false;
        currentSnapshot_.heartRateBpm = 0;
        currentSnapshot_.waveform = 50;
        needsReprime_ = true;
        lastSampleAtUs_ = micros();
        return;
    }

    const uint32_t nowUs = micros();
    uint8_t catchupCount = 0;
    while ((uint32_t)(nowUs - lastSampleAtUs_) >= kSampleIntervalUs && catchupCount < kMaxCatchupSamplesPerUpdate)
    {
        sampleAD8232Now();
        lastSampleAtUs_ += kSampleIntervalUs;
        catchupCount++;
    }

    if (catchupCount == 0)
    {
        sampleAD8232Now();
        lastSampleAtUs_ = nowUs;
    }

    const bool leadsConnected = areLeadsConnected();
    currentSnapshot_.signalReady = leadsConnected;

    if (!leadsConnected)
    {
        currentSnapshot_.heartRateBpm = 0;
        currentSnapshot_.waveform = 50;
        needsReprime_ = true;
        return;
    }

    const float filteredSignalMv = getECGFilteredSignal();
    currentSnapshot_.heartRateBpm = getHeartRate();
    currentSnapshot_.waveform = mapFilteredSignalToChart(filteredSignalMv);
}

SensorSnapshot Ad8232SensorModule::snapshot() const
{
    return currentSnapshot_;
}

int Ad8232SensorModule::mapFilteredSignalToChart(float filteredSignalMv)
{
    if (needsReprime_)
    {
        displayBaseline_ = filteredSignalMv;
        displayFiltered_ = 0.0f;
        envelope_ = 130.0f;
        yScale_ = 230.0f;
        needsReprime_ = false;
    }

    float baselineInput = filteredSignalMv;
    if (baselineInput < -90.0f)
        baselineInput = -90.0f;
    if (baselineInput > 90.0f)
        baselineInput = 90.0f;

    displayBaseline_ = 0.992f * displayBaseline_ + 0.008f * baselineInput;
    const float centered = filteredSignalMv - displayBaseline_;

    const float delta = fabsf(centered - displayFiltered_);
    const float smoothAlpha = (delta > 40.0f) ? 0.28f : 0.58f;
    displayFiltered_ = smoothAlpha * displayFiltered_ + (1.0f - smoothAlpha) * centered;

    const float absDisplay = fabsf(displayFiltered_);
    if (absDisplay > envelope_)
    {
        envelope_ = 0.20f * absDisplay + 0.80f * envelope_;
    }
    else
    {
        envelope_ = 0.005f * absDisplay + 0.995f * envelope_;
    }

    if (envelope_ < 95.0f)
        envelope_ = 95.0f;
    if (envelope_ > 320.0f)
        envelope_ = 320.0f;

    float targetScale = envelope_ * 1.90f;
    if (targetScale < 230.0f)
    {
        targetScale = 230.0f;
    }
    yScale_ = 0.97f * yScale_ + 0.03f * targetScale;

    if (yScale_ < 210.0f)
        yScale_ = 210.0f;
    if (yScale_ > 430.0f)
        yScale_ = 430.0f;

    float limited = displayFiltered_;
    const float maxAbs = 0.95f * yScale_;
    if (limited > maxAbs)
        limited = maxAbs;
    if (limited < -maxAbs)
        limited = -maxAbs;

    int chart200 = static_cast<int>(((limited / yScale_) + 1.0f) * 100.0f);
    if (chart200 < 0)
        chart200 = 0;
    if (chart200 > 200)
        chart200 = 200;

    return chart200;
}
