#include "ina219_sensor_module.h"

#include <math.h>

// ================================================================
// LOOKUP TABLE: Li-ion / LiPo 1S discharge curve
// Nguồn: thực nghiệm + datasheet typical discharge curve
//
// Index i = normalizedV * 20  (ví dụ: 0.5 -> index 10)
// normalizedV = (V - Vmin) / (Vmax - Vmin), clamp [0.0, 1.0]
//
// Table có 21 điểm (0% .. 100% bước 5%)
// Dùng linear interpolation giữa 2 điểm gần nhất
// ================================================================
static const float kLiPoDischargeTable[21] = {
    // % pin:  0    5   10   15   20   25   30   35   40   45   50
    0.00f, 0.05f, 0.09f, 0.14f, 0.20f, 0.26f, 0.32f, 0.38f, 0.44f, 0.50f, 0.56f,
    // % pin: 55   60   65   70   75   80   85   90   95  100
    0.62f, 0.68f, 0.73f, 0.78f, 0.83f, 0.87f, 0.91f, 0.95f, 0.98f, 1.00f};
// Bảng trên map normalized voltage -> % pin
// Ví dụ: normalized=0.56 -> ~50%, normalized=0.91 -> ~85%

// ================================================================
// LOOKUP TABLE: LiFePO4 discharge curve (approximate, 1S normalized)
// vMin=2.8V, vMax=3.6V  -> normalized=(V-2.8)/0.8
// Table có 21 điểm (0% .. 100% bước 5%)
// ================================================================
static const float kLiFeDischargeTable[21] = {
    // % pin:  0     5     10    15    20    25    30    35    40    45    50
    0.00f, 0.19f, 0.31f, 0.38f, 0.44f, 0.48f, 0.50f, 0.53f, 0.55f, 0.56f, 0.58f,
    // % pin: 55    60    65    70    75    80    85    90    95   100
    0.59f, 0.60f, 0.61f, 0.63f, 0.64f, 0.65f, 0.69f, 0.75f, 0.85f, 1.00f};

namespace
{
    constexpr float kLiPo2sVMin = 6.6f;
    constexpr float kLiPo2sVMax = 8.4f;
    constexpr float kChargePercentEmaAlpha = 0.03f;
    constexpr float kDischargePercentEmaAlpha = 0.10f;
    constexpr float kIdleCurrentMa = 30.0f;
    constexpr float kChargerDetectMarginV = 0.05f; // Chỉ dùng fallback khi chưa có chân TP5100
}

INA219SensorModule::INA219SensorModule(uint8_t i2cAddress)
    : i2cAddress_(i2cAddress),
      ready_(false),
      lastReadAtMs_(0),
      lastRetryAtMs_(0),
      filteredVoltage_(0.0f),
      voltageInitialized_(false),
      filteredPercent_(0.0f),
      percentInitialized_(false),
      externalStatusAvailable_(false),
      externalChargingActive_(false),
      externalFullActive_(false),
      lastExternalStatusAtMs_(0),
      lastChargePercentStepAtMs_(0),
      batteryType_(BatteryType::LiPo_1S),
      ina_(i2cAddress)
{
    snapshot_.sensorReady = false;
    snapshot_.busVoltageV = 0.0f;
    snapshot_.currentMa = 0.0f;
    snapshot_.powerMw = 0.0f;
    snapshot_.shuntVoltageV = 0.0f;
    snapshot_.batteryPercent = 0;
    snapshot_.isCharging = false;
    snapshot_.isFull = false;
    snapshot_.chargerPresent = false;
}

bool INA219SensorModule::begin()
{
    ensureI2cConfigured();

    ready_ = ina_.begin();
    lastReadAtMs_ = 0;
    lastRetryAtMs_ = millis();
    filteredVoltage_ = 0.0f;
    voltageInitialized_ = false;
    filteredPercent_ = 0.0f;
    percentInitialized_ = false;
    lastChargePercentStepAtMs_ = millis();

    snapshot_.sensorReady = ready_;

    if (ready_)
    {
        // Cấu hình: 32V bus, 320mV shunt, 12-bit averaging (16 samples)
        // -> giảm nhiễu ADC, đặc biệt khi dùng chung I2C bus với MAX30102
        ina_.setCalibration_32V_1A(); // Dải đo 0-1A, giải tốt nhất cho pin nhỏ
        Serial.printf("[INA219] Init addr=0x%02X, range=32V/1A\n", i2cAddress_);
    }
    else
    {
        Serial.printf("[INA219] Not found at 0x%02X. Check wiring.\n", i2cAddress_);
    }

    return ready_;
}

void INA219SensorModule::update()
{
    const uint32_t nowMs = millis();

    // ---- Retry nếu mất kết nối ----
    if (!ready_)
    {
        if (nowMs - lastRetryAtMs_ < kRetryIntervalMs)
            return;
        lastRetryAtMs_ = nowMs;
        ensureI2cConfigured();
        ready_ = ina_.begin();
        snapshot_.sensorReady = ready_;
        if (ready_)
        {
            ina_.setCalibration_32V_1A();
            voltageInitialized_ = false;
            percentInitialized_ = false;
        }
        return;
    }

    // ---- Throttle đọc ----
    if (nowMs - lastReadAtMs_ < kReadIntervalMs)
        return;
    lastReadAtMs_ = nowMs;

    ensureI2cConfigured();

    static uint32_t lastProbeMs = 0;
    if (nowMs - lastProbeMs >= 2000)
    {
        lastProbeMs = nowMs;
        Wire.beginTransmission(i2cAddress_);
        if (Wire.endTransmission() != 0)
        {
            ready_ = false;
            snapshot_.sensorReady = false;
            return;
        }
    }

    // ---- Đọc từ INA219 ----
    const float shuntV = ina_.getShuntVoltage_mV() / 1000.0f;
    const float busV = ina_.getBusVoltage_V();
    const float loadV = busV + shuntV;
    const float current = ina_.getCurrent_mA();
    const float power = ina_.getPower_mW();

    // ---- Sanity check ----
    static uint8_t invalidCount = 0;
    if (isnan(busV) || busV < 0.1f || busV > 30.0f)
    {
        invalidCount++;
        if (invalidCount >= 3)
        {
            ready_ = false;
            snapshot_.sensorReady = false;
        }
        return;
    }
    invalidCount = 0;

    // ========================================================
    // 1. BÙ TRỪ NỘI TRỞ (IR COMPENSATION)
    // Quy ước: current ÂM khi sạc, DƯƠNG khi xả.
    // Giả sử điện trở nội của pin + dây + mạch bảo vệ là ~0.25 Ohm
    // Khi sạc, V đo bị đội lên -> công thức này (với current âm) sẽ trừ bớt phần đội lên đó.
    // ========================================================
    const float kInternalResistanceOhm = 0.25f;
    float v_rest = loadV + (current / 1000.0f) * kInternalResistanceOhm;

    // ---- EMA filter voltage ----
    if (!voltageInitialized_)
    {
        filteredVoltage_ = v_rest;
        voltageInitialized_ = true;
    }
    else
    {
        // Làm mượt điện áp mạnh tay hơn (alpha = 0.05)
        filteredVoltage_ = 0.05f * v_rest + 0.95f * filteredVoltage_;
    }

    // ---- Tính % pin từ voltage ----
    float vMin = 0.0f;
    float vMax = 0.0f;
    getBatteryRange(vMin, vMax);

    const uint8_t rawPercent = voltageToPercent(filteredVoltage_);
    const bool isChargingByCurrent = (current < -kChargingThreshMa);
    const bool isIdleCurrent = (fabsf(current) <= kIdleCurrentMa);

    // Nếu đã có opto TP5100 thì ưu tiên trạng thái phần cứng, không đoán bằng điện áp charger.
    const bool hasExternalStatus = externalStatusAvailable_ &&
                                   ((nowMs - lastExternalStatusAtMs_) <= kExternalStatusTimeoutMs);
    const bool isChargingByPin = hasExternalStatus && externalChargingActive_ && !externalFullActive_;
    const bool isFullByPin = hasExternalStatus && externalFullActive_;
    const bool isChargerPresent = hasExternalStatus
                                      ? (externalChargingActive_ || externalFullActive_)
                                      : (busV >= (vMax - kChargerDetectMarginV));
    const bool isChargingUi = isChargingByPin || (!hasExternalStatus && (isChargingByCurrent || isChargerPresent));

    if (!percentInitialized_)
    {
        filteredPercent_ = rawPercent;
        snapshot_.batteryPercent = rawPercent; // Lưu mốc ban đầu
        percentInitialized_ = true;
        lastChargePercentStepAtMs_ = nowMs;
    }
    else
    {
        // Khi TP5100 báo đang sạc, điện áp có thể bị đội lên nên chỉ dùng EMA rất chậm.
        const float percentAlpha = isChargingUi ? kChargePercentEmaAlpha : kDischargePercentEmaAlpha;
        filteredPercent_ = percentAlpha * rawPercent + (1.0f - percentAlpha) * filteredPercent_;
    }

    uint8_t targetPercent = static_cast<uint8_t>(filteredPercent_ + 0.5f);

    // ========================================================
    // 2. CHỐT % PIN THEO TRẠNG THÁI TP5100
    // - FULL/STDBY active: chốt 100%
    // - CHRG active: không cho nhảy vọt theo điện áp charger; tăng tối đa 1%/phút
    // - Discharging: chỉ giảm hoặc đứng yên để tránh hồi áp làm % tăng giả
    // ========================================================
    if (isFullByPin)
    {
        snapshot_.batteryPercent = 100;
        filteredPercent_ = 100.0f;
    }
    else if (isChargingUi)
    {
        if (targetPercent > snapshot_.batteryPercent &&
            (nowMs - lastChargePercentStepAtMs_) >= kChargePercentStepMs)
        {
            snapshot_.batteryPercent++;
            lastChargePercentStepAtMs_ = nowMs;
        }
    }
    else
    {
        // Khi xả: % pin chỉ được phép GIẢM hoặc ĐỨNG YÊN.
        if (targetPercent < snapshot_.batteryPercent)
        {
            snapshot_.batteryPercent = targetPercent;
        }
        // Chỉ cho phép tăng nhẹ khi dòng gần idle và chênh lệch lớn, tránh kẹt % sau khi tải giảm.
        else if (isIdleCurrent && targetPercent > snapshot_.batteryPercent + 5)
        {
            snapshot_.batteryPercent = snapshot_.batteryPercent + 1;
        }
    }

    // Đảm bảo an toàn không quá 100%
    if (snapshot_.batteryPercent > 100)
    {
        snapshot_.batteryPercent = 100;
    }

    // ---- Cập nhật snapshot ----
    snapshot_.sensorReady = true;
    snapshot_.busVoltageV = filteredVoltage_;
    snapshot_.shuntVoltageV = shuntV;
    snapshot_.currentMa = current;
    snapshot_.powerMw = power;
    snapshot_.isCharging = isChargingUi;
    snapshot_.isFull = isFullByPin;
    snapshot_.chargerPresent = isChargerPresent;

    // ---- Debug log ----
    static uint32_t lastLogMs = 0;
    if (nowMs - lastLogMs >= 2000)
    {
        lastLogMs = nowMs;
        Serial.printf("[INA219] Vload=%.3fV Vrest=%.3fV Vf=%.3fV UI_BAT=%d%% (target=%u%% raw=%u%%) I=%.1fmA pinCHG=%d pinFULL=%d %s\n",
                      loadV, v_rest, filteredVoltage_,
                      snapshot_.batteryPercent, targetPercent, rawPercent, current,
                      isChargingByPin ? 1 : 0,
                      isFullByPin ? 1 : 0,
                      isFullByPin ? "FULL" : (isChargingUi ? "CHG" : "DSG"));
    }
}

INA219Snapshot INA219SensorModule::snapshot() const
{
    return snapshot_;
}

void INA219SensorModule::setExternalChargeStatus(bool chargingActive, bool fullActive)
{
    externalStatusAvailable_ = true;
    externalChargingActive_ = chargingActive;
    externalFullActive_ = fullActive;
    lastExternalStatusAtMs_ = millis();
}

void INA219SensorModule::setBatteryType(BatteryType type)
{
    batteryType_ = type;
    voltageInitialized_ = false; // Reset filter khi đổi loại pin
}

// ----------------------------------------------------------------
// Private helpers
// ----------------------------------------------------------------

void INA219SensorModule::ensureI2cConfigured()
{
    Wire.setClock(50000);
    Wire.setTimeOut(500);
}

void INA219SensorModule::getBatteryRange(float &vMin, float &vMax) const
{
    switch (batteryType_)
    {
    case BatteryType::LiPo_2S:
        vMin = kLiPo2sVMin;
        vMax = kLiPo2sVMax;
        break;
    case BatteryType::LiPo_3S:
        vMin = 9.0f;
        vMax = 12.6f;
        break;
    case BatteryType::LiFe_2S:
        vMin = 5.6f;
        vMax = 7.2f;
        break;
    case BatteryType::LiPo_1S:
    default:
        // Li-ion/LiPo 1S: cutoff 3.0V (an toàn pin), full 4.2V
        vMin = 3.0f;
        vMax = 4.2f;
        break;
    }
}

uint8_t INA219SensorModule::voltageToPercent(float voltage) const
{
    float vMin, vMax;
    getBatteryRange(vMin, vMax);

    // Clamp
    if (voltage <= vMin)
        return 0;
    if (voltage >= vMax)
        return 100;

    // Normalize về [0.0, 1.0]
    const float normalized = (voltage - vMin) / (vMax - vMin);

    if (batteryType_ == BatteryType::LiFe_2S)
    {
        return lookupLiFePercent(normalized);
    }

    return lookupLiPoPercent(normalized);
}

// static
uint8_t INA219SensorModule::lookupLiPoPercent(float normalizedV)
{
    // Table[i] = normalized voltage tương ứng với i*5% pin
    // Ví dụ: table[10]=0.56 nghĩa là normalized=0.56 -> 50% pin
    // Tìm khoảng [i, i+1] sao cho table[i] <= normalizedV <= table[i+1]

    constexpr int kTableSize = 21;

    if (normalizedV <= kLiPoDischargeTable[0])
        return 0;
    if (normalizedV >= kLiPoDischargeTable[kTableSize - 1])
        return 100;

    for (int i = 0; i < kTableSize - 1; i++)
    {
        if (normalizedV >= kLiPoDischargeTable[i] &&
            normalizedV <= kLiPoDischargeTable[i + 1])
        {
            const float span = kLiPoDischargeTable[i + 1] - kLiPoDischargeTable[i];
            float t = 0.0f;
            if (span > 1e-6f)
                t = (normalizedV - kLiPoDischargeTable[i]) / span;

            // i   -> i*5 %
            // i+1 -> (i+1)*5 %
            const float percentF = (i + t) * 5.0f;
            const int percent = static_cast<int>(percentF + 0.5f);
            if (percent < 0)
                return 0;
            if (percent > 100)
                return 100;
            return static_cast<uint8_t>(percent);
        }
    }

    return 100;
}

// static
uint8_t INA219SensorModule::lookupLiFePercent(float normalizedV)
{
    constexpr int kTableSize = 21;

    if (normalizedV <= kLiFeDischargeTable[0])
        return 0;
    if (normalizedV >= kLiFeDischargeTable[kTableSize - 1])
        return 100;

    for (int i = 0; i < kTableSize - 1; i++)
    {
        if (normalizedV >= kLiFeDischargeTable[i] &&
            normalizedV <= kLiFeDischargeTable[i + 1])
        {
            const float span = kLiFeDischargeTable[i + 1] - kLiFeDischargeTable[i];
            float t = 0.0f;
            if (span > 1e-6f)
                t = (normalizedV - kLiFeDischargeTable[i]) / span;

            const float percentF = (i + t) * 5.0f;
            const int percent = static_cast<int>(percentF + 0.5f);
            if (percent < 0)
                return 0;
            if (percent > 100)
                return 100;
            return static_cast<uint8_t>(percent);
        }
    }

    return 100;
}