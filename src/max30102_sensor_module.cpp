#include "max30102_sensor_module.h"

#include <Wire.h>
#include <math.h>
#include <string.h>
#include "spo2_algorithm.h"
#include "heartRate.h"

Max30102SensorModule::Max30102SensorModule(uint8_t sdaPin, uint8_t sclPin)
    : sdaPin_(sdaPin),
      sclPin_(sclPin),
      ready_(false),
      fingerPresent_(false),
      bufferCount_(0),
      samplesSinceCalc_(0),
      maximSpo2_(0),
      validSpo2_(0),
      maximHeartRate_(0),
      validHeartRate_(0),
      filteredSpo2_(0.0),
      filteredHeartRate_(0.0),
      lastValidHrTime_(0),
      trackedIrMin_(50000.0f),
      trackedIrMax_(70000.0f)
{
}

bool Max30102SensorModule::begin()
{
    Wire.setClock(100000);

    if (!sensor_.begin(Wire, I2C_SPEED_STANDARD))
    {
        ready_ = false;
        fingerPresent_ = false;
        currentSnapshot_.sensorReady = false;
        currentSnapshot_.signalReady = false;
        Serial.println("MAX30102 not found. HR/SpO2 screens stay in placeholder mode.");
        return false;
    }

    const byte ledBrightness = 45;
    const byte sampleAverage = 4;
    const byte ledMode = 2;
    const int sampleRate = 400;
    const int pulseWidth = 411;
    const int adcRange = 16384;

    sensor_.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    sensor_.setPulseAmplitudeGreen(0);
    sensor_.clearFIFO();

    resetMetrics();
    ready_ = true;
    currentSnapshot_.sensorReady = true;
    Serial.println("MAX30102 initialized for live UI updates.");
    return true;
}

void Max30102SensorModule::update()
{
    if (!ready_)
    {
        return;
    }

    sensor_.check();

    bool gotSample = false;
    bool hasValidFingerSample = false;

    while (sensor_.available())
    {
        gotSample = true;

        const uint32_t red = sensor_.getFIFORed();
        const uint32_t ir = sensor_.getFIFOIR();
        sensor_.nextSample();

        // [THÊM LOG DEBUG] In ra RAW IR/RED mỗi giây để bạn dễ kiểm tra
        static unsigned long lastRawLog = 0;
        if (millis() - lastRawLog > 1000)
        {
            lastRawLog = millis();
            Serial.printf("[MAX30102-RAW] IR=%u | RED=%u\n", ir, red);
        }

        // ========== KIỂM TRA NGÓN TAY ==========
        if (ir < kFingerThreshold)
        {
            continue;
        }

        hasValidFingerSample = true;

        // ========== BEAT DETECTION: Dùng thuật toán PBA chính thức ==========
        if (checkForBeat(ir))
        {
            unsigned long now = millis();
            static unsigned long lastBeatTime = 0;
            unsigned long delta = now - lastBeatTime;

            if (delta > 300 && delta < 2000) // 30-200 BPM hợp lý
            {
                float beatsPerMinute = 60000.0f / delta;

                if (filteredHeartRate_ <= 0.0)
                {
                    filteredHeartRate_ = beatsPerMinute;
                }
                else
                {
                    // EMA filter: 70% cũ, 30% mới
                    filteredHeartRate_ = (0.7f * filteredHeartRate_) + (0.3f * beatsPerMinute);
                }

                currentSnapshot_.heartRateBpm = static_cast<int>(filteredHeartRate_ + 0.5);
                lastValidHrTime_ = now;

                static unsigned long lastBeatLog = 0;
                if (now - lastBeatLog >= 5000)
                {
                    lastBeatLog = now;
                    Serial.printf("[MAX30102] HR=%d bpm\n", currentSnapshot_.heartRateBpm);
                }
            }

            lastBeatTime = now;
        }

        // ========== WAVE DISPLAY ==========
        currentSnapshot_.waveform = toPlethScale(ir);

        // ========== LƯU DATA VÀO BUFFER ĐỂ TÍNH SPO2 ==========
        // Kiểm tra IR ổn định: bỏ qua mẫu nếu IR thay đổi >15% so với mẫu trước.
        // Tránh buffer bị "ô nhiễm" khi vừa đặt/nhấc ngón tay (IR nhảy mạnh).
        static uint32_t lastIrForStability = 0;
        const bool irStable = (lastIrForStability == 0) ||
                              (ir > lastIrForStability * 85 / 100 &&
                               ir < lastIrForStability * 115 / 100);
        lastIrForStability = ir;

        if (irStable)
        {
            if (bufferCount_ < kBufferSize)
            {
                redBuffer_[bufferCount_] = red;
                irBuffer_[bufferCount_] = ir;
                bufferCount_++;
            }
            else
            {
                memmove(redBuffer_, redBuffer_ + 1, (kBufferSize - 1) * sizeof(uint32_t));
                memmove(irBuffer_, irBuffer_ + 1, (kBufferSize - 1) * sizeof(uint32_t));
                redBuffer_[kBufferSize - 1] = red;
                irBuffer_[kBufferSize - 1] = ir;
            }
        }
        else
        {
            // IR không ổn định → reset buffer để SpO2 không tính từ data hỗn độn
            bufferCount_ = 0;
            samplesSinceCalc_ = 0;
            filteredSpo2_ = 0.0;
            currentSnapshot_.spo2Percent = 0;
        }

        if (bufferCount_ == kBufferSize)
        {
            samplesSinceCalc_++;
        }
    } // Hết vòng lặp lấy mẫu

    // ========== XỬ LÝ KHI NGÓN TAY RÚT RA ==========
    if (gotSample && !hasValidFingerSample)
    {
        fingerPresent_ = false;
        currentSnapshot_.signalReady = false;
        resetMetrics();
        return;
    }

    if (!hasValidFingerSample)
    {
        return;
    }

    fingerPresent_ = true;
    currentSnapshot_.signalReady = true;

    // ========== TÍNH SPO2 ==========
    if (bufferCount_ >= kBufferSize && samplesSinceCalc_ >= kSamplesToRead)
    {
        samplesSinceCalc_ = 0;

        double rmsSpo2 = calculateRmsSpo2(redBuffer_, irBuffer_, kBufferSize);

        // 0.0 = tín hiệu không hợp lệ (không có ngón tay hoặc AC quá nhỏ)
        if (rmsSpo2 > 0.0)
        {
            if (rmsSpo2 > 100.0) rmsSpo2 = 100.0;
            if (rmsSpo2 < 70.0)  rmsSpo2 = 70.0;

            if (filteredSpo2_ <= 0.0)
            {
                filteredSpo2_ = rmsSpo2;
            }
            else
            {
                filteredSpo2_ = (0.6 * filteredSpo2_) + (0.4 * rmsSpo2);
            }

            currentSnapshot_.spo2Percent = static_cast<int>(filteredSpo2_ + 0.5);

            static unsigned long lastSpo2Log = 0;
            if (millis() - lastSpo2Log >= 5000)
            {
                lastSpo2Log = millis();
                Serial.printf("[MAX30102] SPO2: raw=%.1f%%, filtered=%.1f%%, final=%d%%\n",
                              rmsSpo2, filteredSpo2_, currentSnapshot_.spo2Percent);
            }
        }
    }

    // ========== HOLD LOGIC: GIỮ NHỊP TIM KHI BỊ NHIỄU NGẮN HẠN ==========
    if (millis() - lastValidHrTime_ > 5000)
    {
        currentSnapshot_.heartRateBpm = 0;
        filteredHeartRate_ = 0.0;
    }
}

SensorSnapshot Max30102SensorModule::snapshot() const
{
    return currentSnapshot_;
}

void Max30102SensorModule::resetMetrics()
{
    bufferCount_ = 0;
    samplesSinceCalc_ = 0;
    filteredSpo2_ = 0.0;
    filteredHeartRate_ = 0.0;

    currentSnapshot_.bodyTempC = -1.0f;
    currentSnapshot_.heartRateBpm = 0;
    currentSnapshot_.spo2Percent = 0;
    currentSnapshot_.waveform = 50;
}

double Max30102SensorModule::calculateRmsSpo2(const uint32_t *redBuf, const uint32_t *irBuf, int length) const
{
    if (length <= 0)
    {
        return 0.0;
    }

    double aveRed = 0.0;
    double aveIr = 0.0;
    double sumRedRms = 0.0;
    double sumIrRms = 0.0;

    // Tính trung bình DC
    for (int i = 0; i < length; i++)
    {
        aveRed += redBuf[i];
        aveIr += irBuf[i];
    }
    aveRed /= length;
    aveIr /= length;

    if (aveRed <= 1.0 || aveIr <= 1.0)
    {
        return 0.0; // 0 = không có tín hiệu, không phải SpO2 thật
    }

    // Tính tổng bình phương độ lệch (AC RMS)
    for (int i = 0; i < length; i++)
    {
        const double redDiff = redBuf[i] - aveRed;
        const double irDiff = irBuf[i] - aveIr;
        sumRedRms += redDiff * redDiff;
        sumIrRms += irDiff * irDiff;
    }

    if (sumRedRms <= 0.0 || sumIrRms <= 0.0)
    {
        return 0.0; // AC quá nhỏ — ngón tay không đặt đúng hoặc tín hiệu phẳng
    }

    // Tính AC RMS
    double acRed = sqrt(sumRedRms / length);
    double acIr = sqrt(sumIrRms / length);

    // AC/DC ratio
    double ratioRed = acRed / aveRed;
    double ratioIr = acIr / aveIr;

    // Công thức tính SpO2 từ AC/DC ratio R = (AC_Red/DC_Red) / (AC_IR/DC_IR)
    double ratio = ratioRed / ratioIr;

    // Quadratic empirical formula (Maxim AN6409, calibrated for MAX30102 660nm/940nm).
    // Chính xác hơn linear "110-25R" đặc biệt ở vùng 95-100% và 85-94%.
    // Sai số ±2% so với pulse oximeter chuẩn; cần calibration phần cứng để đạt ±1%.
    double spo2 = -45.060 * ratio * ratio + 30.354 * ratio + 94.845;

    return spo2;
}

int Max30102SensorModule::toPlethScale(uint32_t irValue)
{
    float ir = static_cast<float>(irValue);

    // 1. Lọc High-Pass (Khử DC - Baseline Wander)
    // Kéo toàn bộ sóng về dao động quanh mốc 0, giúp sóng luôn nằm chính giữa màn hình
    static float dcFilter = 0.0f;
    if (dcFilter == 0.0f)
        dcFilter = ir; // Khởi tạo mốc ban đầu
    dcFilter = 0.98f * dcFilter + 0.02f * ir;
    float acSignal = ir - dcFilter; // acSignal giờ chỉ còn là độ nhấp nhô của mạch máu

    // 2. Theo dõi biên độ (Auto-Gain thông minh)
    // Bám đỉnh/đáy ngay lập tức, và tự động hao mòn 2% về 0 để sóng luôn căng đều dải 0-100
    if (acSignal > trackedIrMax_)
        trackedIrMax_ = acSignal;
    else
        trackedIrMax_ *= 0.98f;

    if (acSignal < trackedIrMin_)
        trackedIrMin_ = acSignal;
    else
        trackedIrMin_ *= 0.98f;

    float span = trackedIrMax_ - trackedIrMin_;

    // 3. Chống nhiễu khi rút ngón tay
    if (span < 10.0f)
        return 50;

    // 4. Chuẩn hóa về dải 0.0 -> 1.0
    float normalized = (acSignal - trackedIrMin_) / span;
    if (normalized < 0.0f)
        normalized = 0.0f;
    if (normalized > 1.0f)
        normalized = 1.0f;

    // 5. Đảo chiều sóng (Máu bơm tới -> Hút tia IR -> Giá trị IR giảm)
    // Đảo lại để mỗi lần tim đập, đồ thị sẽ nhô đỉnh lên trên
    return static_cast<int>((1.0f - normalized) * 100.0f);
}