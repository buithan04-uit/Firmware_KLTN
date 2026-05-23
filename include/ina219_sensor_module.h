#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// ================================================================
// INA219SensorModule
// Đọc voltage, current, power từ INA219 và tính % pin chính xác
//
// Kết nối: SDA=21, SCL=22 (chung với MLX90614)
// Địa chỉ I2C mặc định: 0x40 (A0=GND, A1=GND)
//
// Pin supported:
//   - Li-ion / LiPo 1S: 3.0V (0%) ~ 4.2V (100%)  [DEFAULT]
//   - Li-ion / LiPo 2S: 6.6V (0%) ~ 8.4V (100%)
//   - LiFePO4 2S: 5.6V (0%) ~ 7.2V (100%)
//   - Thay doi qua setBatteryType()
// ================================================================

struct INA219Snapshot
{
    bool sensorReady;       // INA219 đã kết nối chưa
    float busVoltageV;      // Điện áp bus / điện áp đã lọc (V)
    float currentMa;        // Dòng điện (mA), âm = đang sạc nếu INA219 đấu đúng chiều
    float powerMw;          // Công suất (mW)
    float shuntVoltageV;    // Shunt voltage (V)
    uint8_t batteryPercent; // % pin (0–100)
    bool isCharging;        // Đang sạc thật theo TP5100 CHRG hoặc fallback INA219
    bool isFull;            // TP5100 báo FULL/STDBY
    bool chargerPresent;    // Có trạng thái sạc/full từ TP5100 hoặc fallback
};

enum class BatteryType
{
    LiPo_1S, // 3.0V – 4.2V  (mặc định)
    LiPo_2S, // 6.4V – 8.4V
    LiPo_3S, // 9.0V – 12.6V
    LiFe_2S, // 5.6V – 7.2V  (LiFePO4 2S, 3.2V/cell nominal)
};

class INA219SensorModule
{
public:
    // sdaPin/sclPin chỉ để log, Wire đã được init bởi hệ thống
    explicit INA219SensorModule(uint8_t i2cAddress = 0x40);

    bool begin();
    void update(); // Gọi trong loop hoặc background task
    INA219Snapshot snapshot() const;

    void setBatteryType(BatteryType type);

    // Trạng thái đọc từ TP5100 qua opto TLP521-2/PC817.
    // active LOW ở GPIO đã được xử lý bên ngoài: truyền true khi chân GPIO đọc LOW.
    void setExternalChargeStatus(bool chargingActive, bool fullActive);

    // Lấy giá trị raw mới nhất (không cần snapshot)
    float busVoltageV() const { return snapshot_.busVoltageV; }
    float currentMa() const { return snapshot_.currentMa; }
    float powerMw() const { return snapshot_.powerMw; }
    uint8_t batteryPercent() const { return snapshot_.batteryPercent; }
    bool isCharging() const { return snapshot_.isCharging; }
    bool isFull() const { return snapshot_.isFull; }
    bool chargerPresent() const { return snapshot_.chargerPresent; }
    bool sensorReady() const { return ready_; }

private:
    // ---- Cấu hình ----
    static constexpr uint32_t kReadIntervalMs = 500;   // Đọc 2 lần/giây
    static constexpr uint32_t kRetryIntervalMs = 3000; // Thử lại nếu mất kết nối
    static constexpr float kChargingThreshMa = 10.0f;  // > 10mA = đang sạc

    // EMA filter cho voltage (giảm ripple do load thay đổi)
    // Alpha lớn = bám nhanh hơn, alpha nhỏ = mượt hơn
    static constexpr float kVoltageEmaAlpha = 0.15f;
    static constexpr float kPercentEmaAlpha = 0.20f;
    static constexpr uint32_t kExternalStatusTimeoutMs = 3000;
    static constexpr uint32_t kChargePercentStepMs = 60000; // Khi đang sạc, tăng tối đa 1%/phút để tránh nhảy ảo

    // ---- State ----
    uint8_t i2cAddress_;
    bool ready_;
    uint32_t lastReadAtMs_;
    uint32_t lastRetryAtMs_;
    float filteredVoltage_;
    bool voltageInitialized_;
    float filteredPercent_;
    bool percentInitialized_;
    bool externalStatusAvailable_;
    bool externalChargingActive_;
    bool externalFullActive_;
    uint32_t lastExternalStatusAtMs_;
    uint32_t lastChargePercentStepAtMs_;
    BatteryType batteryType_;
    INA219Snapshot snapshot_;
    Adafruit_INA219 ina_;

    // ---- Helpers ----
    void ensureI2cConfigured();
    uint8_t voltageToPercent(float voltage) const;
    void getBatteryRange(float &vMin, float &vMax) const;

    // Lookup table: Li-ion/LiPo discharge curve (thực nghiệm)
    // Chính xác hơn linear mapping rất nhiều vì pin không xả tuyến tính
    static uint8_t lookupLiPoPercent(float normalizedV);
    static uint8_t lookupLiFePercent(float normalizedV);
};
