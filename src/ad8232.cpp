/*
 * ===================================================================
 * AD8232 ECG SENSOR WITH ADS1115 ADC
 * ===================================================================
 *
 * Tính năng:
 * - Sử dụng ADS1115 16-bit ADC để đọc tín hiệu chính xác hơn
 * - Bộ lọc nhiễu đa tầng:
 *   + Moving Average Filter (khử nhiễu tần số cao)
 *   + Median Filter (loại bỏ spike)
 *   + High-pass filter (loại bỏ baseline drift)
 * - Phát hiện lead-off (dây điện cực bị tuột)
 * - Tính nhịp tim real-time
 *
 * Kết nối phần cứng:
 * AD8232:
 *   - OUTPUT -> ADS1115 A0
 *   - LO+ -> GPIO 13
 *   - LO- -> GPIO 14
 *   - 3.3V, GND
 *
 * ADS1115:
 *   - SDA -> GPIO 4
 *   - SCL -> GPIO 5
 *   - VDD -> 3.3V
 *   - GND -> GND
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "ecg_ai_bridge.h"

// ==========================================
// CẤU HÌNH CHÂN
// ==========================================
#define LO_PLUS_PIN 13  // Lead-off detection positive
#define LO_MINUS_PIN 14 // Lead-off detection negative
#define I2C_SDA 4
#define I2C_SCL 5

// ==========================================
// CẤU HÌNH BỘ LỌC
// ==========================================
#define MOVING_AVG_SIZE 4    // MA filter (4 mẫu = cân bằng nhanh/mượt)
#define MEDIAN_FILTER_SIZE 5 // Median filter (5 = loại spike hiệu quả)
#define SAMPLE_RATE 250      // Tần số lấy mẫu (Hz) - chuẩn ECG
#define ADC_MV_PER_BIT 0.125f
#define ADC_MIN_VALID 250
#define ADC_MAX_VALID 32767
#define ECG_DISPLAY_POLARITY 1.0f // set to -1.0f if MCP6002/lead orientation makes R-peaks point down

// High-pass filter loại bỏ DC offset & baseline drift
#define HPF_ALPHA 0.983f // fc ≈ 0.7Hz tại 250Hz, giảm baseline wander tốt hơn

// Smart LPF: mượt khi nền phẳng, giảm lọc khi qua QRS để giữ chi tiết
// FIX2: α_quiet=0.82 (cutoff≈7Hz) giữ đủ chi tiết sóng P/T; α_QRS=0.55 cho QRS peak
// LPF_ALPHA_QRS không nên quá thấp (<0.50) vì sẽ bypass filter → spike passthrough
#define LPF_ALPHA_QUIET 0.82f
#define LPF_ALPHA_QRS 0.55f
// OPT-2: Tăng ngưỡng từ 25→60mV để tránh LPF switching liên tục do noise floor
// 60mV/sample tương ứng sườn QRS thật sau notch filter tại 250Hz
#define QRS_SLOPE_THRESHOLD_MV 60.0f
// Spike gate: giới hạn bước nhảy của filteredSignal để ngăn transient HPF gây spike
// OPT-1: Giảm từ 450→200mV — QRS thật tối đa ~200mV/sample, spike log >400 đều là artefact
#define FILT_MAX_STEP_MV 200.0f

// 50Hz notch @ Fs=250Hz (biquad, Q~10) để giảm nhiễu điện lưới
#define NOTCH_B0 0.95459f
#define NOTCH_B1 -0.58970f
#define NOTCH_B2 0.95459f
#define NOTCH_A1 -0.58970f
#define NOTCH_A2 0.90919f

// Nhánh HR riêng: lọc nhẹ để giảm false peak nhưng vẫn giữ QRS
// FIX: 0.72→0.55, cutoff ~34Hz tại 250Hz, peak QRS rõ hơn cho detector
#define HR_LPF_ALPHA 0.55f

// ==========================================
// PHÁT HIỆN NHỊP TIM (R-R Interval)
// ==========================================
// OPT-6: Tăng HR_REFRACTORY_MS 380→450ms để tránh đếm sóng T giả
#define HR_REFRACTORY_MS 450 // 450ms → tối đa ~133 BPM, giảm T-wave false detection
#define HR_TIMEOUT_MS 5000   // 5s timeout, ổn định hơn khi tín hiệu yếu ngắn hạn
#define HR_MIN_BPM 40
#define HR_MAX_BPM 133
#define HR_ARTIFACT_REJECT_MV 900.0f
#define HR_STARTUP_BLANK_MS 600 // FIX: rút ngắn 850→600ms để bắt nhịp sớm hơn
// OPT-6: Tăng RR buffer 6→8 để trung bình nhiều nhịp hơn, HR ổn định hơn
#define HR_RR_BUFFER 8
// OPT-6: Tăng LP alpha 0.60→0.75 để HR display ổn định, giảm nhảy số trên LCD
#define HR_BPM_LP_ALPHA 0.75f

// ==========================================
// BIẾN TOÀN CỤC
// ==========================================
Adafruit_ADS1115 ads; // ADS1115 ADC
static TwoWire &adsI2C = Wire1;

// Bộ đệm bộ lọc
float movingAvgBuffer[MOVING_AVG_SIZE];
float medianBuffer[MEDIAN_FILTER_SIZE];
int movingAvgIndex = 0;
int medianIndex = 0;
bool buffersInitialized = false;

// High-pass filter variables
float hpfPrevInput = 0;
float hpfPrevOutput = 0;

// Low-pass filter variable
float lpfPrevOutput = 0;
float hrLpfPrevOutput = 0;

// Notch filter states
float notch_x1 = 0;
float notch_x2 = 0;
float notch_y1 = 0;
float notch_y2 = 0;

// Phát hiện nhịp tim (R-R interval method)
unsigned long lastRPeakTime = 0;
unsigned long lastSignalTime = 0;
int heartRate = 0;
float rrIntervals[HR_RR_BUFFER];
int rrIndex = 0;
int rrCount = 0;
float derivBaseline = 0;
float hrEnvelope = 18.0f;
bool beatDetected = false;
unsigned long hrFirstValidMs = 0;
float hrPrevAbs1 = 0;
float hrPrevAbs2 = 0;
float hrInstantBpmLp = 0;
bool hrInstantInit = false;

// Trạng thái
bool leadsConnected = false;
float rawSignal = 0;
float filteredSignal = 0;
float hrInputSignal = 0;
// FIX: displayBaseline đã bị xoá — baseline drift xử lý DUY NHẤT bởi HPF (fc≈0.7Hz)
// Không còn subtract baseline thêm ở ad8232.cpp hay ui.cpp nữa.
bool adsAvailable = false;

// ==========================================
// HÀM KHỞI TẠO
// ==========================================
void initAD8232()
{
    Serial.println("=== Khởi tạo AD8232 với ADS1115 ===");
    pinMode(LO_PLUS_PIN, INPUT_PULLDOWN);
    pinMode(LO_MINUS_PIN, INPUT_PULLDOWN);

    // 100kHz Standard-mode: ổn định hơn với dây dài (>20cm), tránh Wire Error 263
    adsI2C.begin(I2C_SDA, I2C_SCL);
    adsI2C.setClock(100000);
    adsI2C.setTimeOut(50); // timeout 50ms, tránh block lâu khi bus bị treo
    delay(50);

    // Retry 3 lần để đảm bảo ADS1115 kịp khởi động (một số board cần thêm thời gian)
    bool found = false;
    for (int attempt = 1; attempt <= 3 && !found; attempt++)
    {
        found = ads.begin(0x48, &adsI2C);
        if (!found)
        {
            Serial.printf("ADS1115 attempt %d/3 failed, retrying...\n", attempt);
            delay(50);
        }
    }

    if (!found)
    {
        Serial.println("ERROR: Không tìm thấy ADS1115!");
        Serial.println("Kiểm tra kết nối I2C:");
        Serial.println("  - SDA -> GPIO 4");
        Serial.println("  - SCL -> GPIO 5");
        Serial.println("  - VDD -> 3.3V");
        Serial.println("  - GND -> GND");
        Serial.println("⚠️  Continue without ADS1115 (UI vẫn chạy, ECG = 0)");
        adsAvailable = false;
        leadsConnected = false;
        rawSignal = 0;
        filteredSignal = 0;
        hrInputSignal = 0;
        heartRate = 0;
        return;
    }

    adsAvailable = true;

    // 250 SPS: conversion time 4ms, đủ cho 250Hz sampling, ít lỗi I2C hơn 475SPS
    // startContinuous: ADS1115 tự chuyển đổi liên tục, đọc bằng getLastConversionResults()
    // giúp tránh lỗi "kết quả cũ" khi đọc quá nhanh so với conversion time
    ads.setGain(GAIN_ONE);                // ±4.096V range
    ads.setDataRate(RATE_ADS1115_250SPS); // 250 SPS, conversion time ~4ms
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);

    Serial.println("⚙️  ADS1115 Config:");
    Serial.println("   - Gain: ±4.096V (0.125 mV/bit)");
    Serial.println("   - Rate: 250 SPS (continuous mode A0)");
    Serial.println("✓ ADS1115 khởi tạo thành công");
    Serial.println("  Continuous mode on A0, đang lấy mẫu...");

    // Pre-fill filter buffers với giá trị ADC đầu tiên đọc từ continuous mode
    delay(10); // Chờ 1 conversion (4ms @ 250SPS)
    int16_t adc0 = ads.getLastConversionResults();
    float initMV = (adc0 > 2000 && adc0 < 32000) ? (adc0 * ADC_MV_PER_BIT) : 1650.0f;

    for (int i = 0; i < MOVING_AVG_SIZE; i++)
    {
        movingAvgBuffer[i] = initMV;
    }
    for (int i = 0; i < MEDIAN_FILTER_SIZE; i++)
    {
        medianBuffer[i] = initMV;
    }

    // Pre-load HPF state: input = initMV, output = 0 → no step response
    hpfPrevInput = initMV;
    hpfPrevOutput = 0;
    lpfPrevOutput = 0; // LPF chạy sau HPF nên preload ở 0 để không tạo bước nhảy giả
    hrLpfPrevOutput = 0;
    // FIX: displayBaseline đã bị xoá, không cần init nữa
    notch_x1 = initMV;
    notch_x2 = initMV;
    notch_y1 = initMV;
    notch_y2 = initMV;
    buffersInitialized = true;

    Serial.print("✓ Bộ lọc pre-loaded: ");
    Serial.print(initMV, 1);
    Serial.println(" mV");
    Serial.print("✓ Tần số lấy mẫu: ");
    Serial.print(SAMPLE_RATE);
    Serial.println(" Hz");
    Serial.println("========================================");
}

// ==========================================
// BỘ LỌC MOVING AVERAGE (Khử nhiễu cao tần)
// ==========================================
float applyMovingAverage(float newValue)
{
    movingAvgBuffer[movingAvgIndex] = newValue;
    movingAvgIndex = (movingAvgIndex + 1) % MOVING_AVG_SIZE;

    float sum = 0;
    for (int i = 0; i < MOVING_AVG_SIZE; i++)
    {
        sum += movingAvgBuffer[i];
    }

    return sum / MOVING_AVG_SIZE;
}

// ==========================================
// BỘ LỌC MEDIAN (Loại bỏ spike)
// ==========================================
float applyMedianFilter(float newValue)
{
    medianBuffer[medianIndex] = newValue;
    medianIndex = (medianIndex + 1) % MEDIAN_FILTER_SIZE;

    // Copy buffer để sort
    float sortedBuffer[MEDIAN_FILTER_SIZE];
    for (int i = 0; i < MEDIAN_FILTER_SIZE; i++)
    {
        sortedBuffer[i] = medianBuffer[i];
    }

    // Bubble sort (đơn giản cho mảng nhỏ)
    for (int i = 0; i < MEDIAN_FILTER_SIZE - 1; i++)
    {
        for (int j = 0; j < MEDIAN_FILTER_SIZE - i - 1; j++)
        {
            if (sortedBuffer[j] > sortedBuffer[j + 1])
            {
                float temp = sortedBuffer[j];
                sortedBuffer[j] = sortedBuffer[j + 1];
                sortedBuffer[j + 1] = temp;
            }
        }
    }

    // Trả về giá trị median
    return sortedBuffer[MEDIAN_FILTER_SIZE / 2];
}

// ==========================================
// HIGH-PASS FILTER (Loại bỏ baseline drift)
// ==========================================
// y[n] = alpha * (y[n-1] + x[n] - x[n-1])
// alpha = RC / (RC + dt) với fc = 1/(2*pi*RC)
float applyHighPassFilter(float input)
{
    float output = HPF_ALPHA * (hpfPrevOutput + input - hpfPrevInput);

    hpfPrevInput = input;
    hpfPrevOutput = output;

    return output;
}

// ==========================================
// LOW-PASS FILTER (Exponential Moving Average)
// ==========================================
// Smooths signal, removes high-frequency noise >40Hz
// y[n] = alpha * y[n-1] + (1 - alpha) * x[n]
// OPT-2: Thêm hold counter để tránh switching liên tục (hysteresis)
float applyLowPassFilter(float input)
{
    static float prevInput = 0;
    static uint8_t qrsHoldCount = 0;
    float slope = fabsf(input - prevInput);
    prevInput = input;

    if (slope > QRS_SLOPE_THRESHOLD_MV)
    {
        qrsHoldCount = 6; // giữ QRS mode 6 mẫu (~24ms) sau khi slope xuống
    }
    else if (qrsHoldCount > 0)
    {
        qrsHoldCount--;
    }

    float alpha = (qrsHoldCount > 0) ? LPF_ALPHA_QRS : LPF_ALPHA_QUIET;
    lpfPrevOutput = alpha * lpfPrevOutput + (1.0f - alpha) * input;
    return lpfPrevOutput;
}

float applyNotch50Hz(float input)
{
    float y = NOTCH_B0 * input + NOTCH_B1 * notch_x1 + NOTCH_B2 * notch_x2 - NOTCH_A1 * notch_y1 - NOTCH_A2 * notch_y2;

    notch_x2 = notch_x1;
    notch_x1 = input;
    notch_y2 = notch_y1;
    notch_y1 = y;

    return y;
}

float applyHeartRateLPF(float input)
{
    hrLpfPrevOutput = HR_LPF_ALPHA * hrLpfPrevOutput + (1.0f - HR_LPF_ALPHA) * input;
    return hrLpfPrevOutput;
}

// ==========================================
// KIỂM TRA LEAD-OFF
// ==========================================
bool checkLeadsConnected()
{
    // Debounce lead-off inputs to avoid false disconnect flaps from electrical noise.
    static uint8_t connectedTicks = 0;
    static uint8_t disconnectedTicks = 0;
    static bool stableConnected = false;

    const bool loPlus = digitalRead(LO_PLUS_PIN);
    const bool loMinus = digitalRead(LO_MINUS_PIN);
    const bool instantConnected = !(loPlus || loMinus);

    if (instantConnected)
    {
        if (connectedTicks < 5)
            connectedTicks++;
        disconnectedTicks = 0;
    }
    else
    {
        if (disconnectedTicks < 5)
            disconnectedTicks++;
        connectedTicks = 0;
    }

    if (connectedTicks >= 3)
        stableConnected = true;
    else if (disconnectedTicks >= 3)
        stableConnected = false;

    return stableConnected;
}

// ==========================================
// ĐỌC VÀ LỌC TÍN HIỆU
// ==========================================
void readAndFilterECG()
{
    static uint8_t invalidStreak = 0;
    static uint8_t hardFailStreak = 0;
    static unsigned long readBackoffUntilMs = 0;
    static unsigned long lastBusRecoveryAtMs = 0;
    static float lastRawMv = 1650.0f;

    if (!adsAvailable)
    {
        leadsConnected = false;
        rawSignal = 0;
        filteredSignal = 0;
        heartRate = 0;
        return;
    }

    // Kiểm tra lead-off
    leadsConnected = checkLeadsConnected();

    if (!leadsConnected)
    {
        rawSignal = 0;
        filteredSignal = 0;
        heartRate = 0;
        rrIndex = 0;
        rrCount = 0;
        derivBaseline = 0;
        hrFirstValidMs = 0;
        hrPrevAbs1 = 0;
        hrPrevAbs2 = 0;
        hrInstantBpmLp = 0;
        hrInstantInit = false;
        lastRPeakTime = 0;
        beatDetected = false;
        buffersInitialized = false;

        static unsigned long lastWarning = 0;
        if (millis() - lastWarning >= 5000)
        {
            lastWarning = millis();
            Serial.println("[ECG] LEADS DISCONNECTED");
        }

        ecgAiBridge.reset();
        return;
    }

    // Back off sau nhiều lần lỗi liên tiếp
    if (millis() < readBackoffUntilMs)
    {
        return; // Không nhét mẫu cũ, bỏ qua hoàn toàn frame này
    }

    // Đọc kết quả conversion cuối (continuous mode, không trigger mới → không block)
    int16_t adcValue = ads.getLastConversionResults();

    // Validate: output AD8232/MCP6002 thường quanh 1.5–1.8V.
    // Với mạch 3.3V analog, mẫu gần GND hoặc gần rail thường là lead-off/saturation/glitch.
    const int16_t ADC_VALID_LOW = 800;    // ~100 mV, dưới vùng hoạt động thực tế của AD8232/MCP6002
    const int16_t ADC_VALID_HIGH = 27000; // ~3.375V, coi như gần rail 3.3V → bỏ mẫu bão hoà
    if (adcValue < ADC_VALID_LOW || adcValue >= ADC_VALID_HIGH)
    {
        // Mẫu không hợp lệ: KHÔNG dùng giá trị cũ, bỏ qua hoàn toàn
        invalidStreak++;
        hardFailStreak++;

        if (invalidStreak >= 3)
        {
            readBackoffUntilMs = millis() + 20; // backoff 20ms
            invalidStreak = 0;
        }

        // Recover I2C bus nếu lỗi kéo dài
        if (hardFailStreak >= 20 && (millis() - lastBusRecoveryAtMs) >= 1000)
        {
            lastBusRecoveryAtMs = millis();
            Serial.println("[ECG] I2C bus recovery attempt...");
            adsI2C.end();
            delay(10);
            adsI2C.begin(I2C_SDA, I2C_SCL);
            adsI2C.setClock(100000);
            adsI2C.setTimeOut(50);
            if (ads.begin(0x48, &adsI2C))
            {
                ads.setGain(GAIN_ONE);
                ads.setDataRate(RATE_ADS1115_250SPS);
                ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
                Serial.println("[ECG] I2C bus recovered.");
            }
            hardFailStreak = 0;
        }

        // Không cập nhật rawSignal/filteredSignal → lcd.cpp sẽ detect newRawSample=false và skip
        return;
    }

    // Mẫu hợp lệ: reset streak
    invalidStreak = 0;
    hardFailStreak = 0;

    float rawMv = adcValue * ADC_MV_PER_BIT;

    // Suppress spike: giới hạn bước nhảy 1 mẫu (giữ đặc trưng QRS nhưng loại glitch)
    float rawStep = rawMv - lastRawMv;
    const float maxRawStepMv = 300.0f; // Nới rộng hơn để không cắt đỉnh QRS cao
    if (rawStep > maxRawStepMv)
        rawMv = lastRawMv + maxRawStepMv;
    else if (rawStep < -maxRawStepMv)
        rawMv = lastRawMv - maxRawStepMv;
    lastRawMv = rawMv;
    rawSignal = rawMv;

    // Re-prime filters sau khi vừa lead-off/reconnect hoặc sau lỗi bus để tránh HPF transient lớn.
    if (!buffersInitialized)
    {
        for (int i = 0; i < MOVING_AVG_SIZE; i++) movingAvgBuffer[i] = rawMv;
        for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) medianBuffer[i] = rawMv;
        movingAvgIndex = 0;
        medianIndex = 0;
        hpfPrevInput = rawMv;
        hpfPrevOutput = 0.0f;
        lpfPrevOutput = 0.0f;
        hrLpfPrevOutput = 0.0f;
        notch_x1 = rawMv;
        notch_x2 = rawMv;
        notch_y1 = rawMv;
        notch_y2 = rawMv;
        filteredSignal = 0.0f;
        hrInputSignal = 0.0f;
        buffersInitialized = true;
        return;
    }

    //     // AI PATH: push vào bridge TRƯỚC khi qua spike gate display
    //     // rawMv đã qua step-clamp 300mV nhưng chưa qua adaptive LPF.
    //     // Bridge có filter riêng (HPF+Notch+LPF cố định), không spike gate.
    if (leadsConnected)
        ecgAiBridge.pushSample(rawMv);

    // Nhánh lọc thông minh:
    // - notch 50Hz để triệt nhiễu điện lưới
    // - hrInputSignal: lọc nhẹ riêng cho detector
    // - filteredSignal: làm mượt thích nghi cho hiển thị
    // Dùng MA trước median để giảm nhiễu rời rạc và hạn chế peak giả
    float ma = applyMovingAverage(rawSignal);
    float conditioned = applyMedianFilter(ma);
    float notch = applyNotch50Hz(conditioned);
    float ac = applyHighPassFilter(notch) * ECG_DISPLAY_POLARITY;
    hrInputSignal = applyHeartRateLPF(ac);
    filteredSignal = applyLowPassFilter(ac);

    // Spike gate: giới hạn bước nhảy tối đa của filteredSignal
    // Lý do: HPF tạo transient lớn khi có artefact (electrode pop, muscle noise).
    // LPF_ALPHA_QRS=0.55 chưa đủ khử hết → spike lọt qua đến chart (1000+ mV).
    // Gate này giữ lại đặc trưng QRS thực (≤450 mV/sample) nhưng chặn artefact đột ngột.
    {
        static float prevFilt = 0.0f;
        float step = filteredSignal - prevFilt;
        if (step > FILT_MAX_STEP_MV)
            filteredSignal = prevFilt + FILT_MAX_STEP_MV;
        else if (step < -FILT_MAX_STEP_MV)
            filteredSignal = prevFilt - FILT_MAX_STEP_MV;
        prevFilt = filteredSignal;
    }

    // FIX: XOÁ displayBaseline subtraction ở đây.
    // Baseline drift đã được xử lý bởi HPF (fc≈0.7Hz) → đủ rồi, không cần subtract thêm.

    // Cập nhật thời gian nhận tín hiệu
    lastSignalTime = millis();
}

// ==========================================
// PHÁT HIỆN NHỊP TIM
// ==========================================
void detectHeartRate()
{
    if (!leadsConnected || !buffersInitialized)
    {
        heartRate = 0;
        rrCount = 0;
        rrIndex = 0;
        derivBaseline = 0;
        hrEnvelope = 18.0f;
        hrFirstValidMs = 0;
        hrPrevAbs1 = 0;
        hrPrevAbs2 = 0;
        hrInstantBpmLp = 0;
        hrInstantInit = false;
        lastRPeakTime = 0;
        return;
    }

    unsigned long now = millis();
    // OPT-3: Bỏ fabsf() — chỉ detect đỉnh DƯƠNG (sóng R thật sau HPF).
    // fabsf() biến sóng T và noise âm thành peak dương giả → đếm nhịp sai.
    // Sóng R sau HPF luôn dương khi điện cực đặt đúng (LA-RA hoặc single-lead).
    // Use QRS energy so HR detection survives biphasic or inverted ECG.
    float absCurr = fabsf(hrInputSignal);

    if (hrFirstValidMs == 0)
        hrFirstValidMs = now;
    if (now - hrFirstValidMs < HR_STARTUP_BLANK_MS)
    {
        heartRate = 0;
        hrPrevAbs2 = hrPrevAbs1;
        hrPrevAbs1 = absCurr;
        return;
    }

    // Detector dùng hrInputSignal có dấu để chỉ nhận R-peak dương.
    // Envelope vẫn dùng trị tuyệt đối để tự thích nghi biên độ nền.
    float absCurrForEnv = absCurr;
    derivBaseline = derivBaseline * 0.996f + absCurrForEnv * 0.004f;
    hrEnvelope = hrEnvelope * 0.994f + absCurrForEnv * 0.006f;

    float thresholdByDeriv = derivBaseline * 1.50f;
    float thresholdByEnv = hrEnvelope * 1.35f;
    float threshold = fmaxf(thresholdByDeriv, thresholdByEnv);
    if (threshold < 6.0f)
        threshold = 6.0f;
    if (threshold > 70.0f)
        threshold = 70.0f;

    bool localPeak = (hrPrevAbs1 > hrPrevAbs2) && (hrPrevAbs1 > absCurr);
    float prominence = hrPrevAbs1 - fminf(hrPrevAbs2, absCurr);
    float prominenceGate = fmaxf(4.0f, threshold * 0.20f);
    bool prominenceOk = prominence > prominenceGate;
    bool timeOk = (lastRPeakTime == 0) || (now - lastRPeakTime >= HR_REFRACTORY_MS);
    bool artifactOk = hrPrevAbs1 < HR_ARTIFACT_REJECT_MV;

    if (localPeak && prominenceOk && hrPrevAbs1 > threshold && !artifactOk)
    {
        hrPrevAbs2 = hrPrevAbs1;
        hrPrevAbs1 = absCurr;
        return;
    }

    if (localPeak && prominenceOk && hrPrevAbs1 > threshold && timeOk)
    {
        unsigned long rrInterval = (lastRPeakTime == 0) ? 0 : (now - lastRPeakTime);
        lastRPeakTime = now;
        beatDetected = true;

        if (rrInterval >= (60000UL / HR_MAX_BPM) && rrInterval <= (60000UL / HR_MIN_BPM))
        {
            float instBpm = 60000.0f / (float)rrInterval;
            if (!hrInstantInit)
            {
                hrInstantBpmLp = instBpm;
                hrInstantInit = true;
            }
            else
            {
                hrInstantBpmLp = HR_BPM_LP_ALPHA * hrInstantBpmLp + (1.0f - HR_BPM_LP_ALPHA) * instBpm;
            }

            if (rrCount >= 3)
            {
                float avgRRForGate = 0;
                for (int i = 0; i < rrCount; i++)
                    avgRRForGate += rrIntervals[i];
                avgRRForGate /= rrCount;

                float minGate = avgRRForGate * 0.58f;
                float maxGate = avgRRForGate * 1.75f;
                if (rrInterval < minGate || rrInterval > maxGate)
                {
                    hrPrevAbs2 = hrPrevAbs1;
                    hrPrevAbs1 = absCurr;
                    return;
                }
            }

            rrIntervals[rrIndex] = (float)rrInterval;
            rrIndex = (rrIndex + 1) % HR_RR_BUFFER;
            if (rrCount < HR_RR_BUFFER)
                rrCount++;

            if (rrCount < 3)
            {
                int bpmEarly = (int)(hrInstantBpmLp + 0.5f);
                heartRate = (bpmEarly >= HR_MIN_BPM && bpmEarly <= HR_MAX_BPM) ? bpmEarly : 0;
            }
            else
            {
                float avgRR = 0;
                for (int i = 0; i < rrCount; i++)
                    avgRR += rrIntervals[i];
                avgRR /= rrCount;

                int bpm = (int)((0.70f * (60000.0f / avgRR) + 0.30f * hrInstantBpmLp) + 0.5f);
                heartRate = (bpm >= HR_MIN_BPM && bpm <= HR_MAX_BPM) ? bpm : 0;
            }
        }
    }

    hrPrevAbs2 = hrPrevAbs1;
    hrPrevAbs1 = absCurr;

    // Timeout: không có nhịp đủ lâu thì reset detector
    if (now - lastRPeakTime > HR_TIMEOUT_MS)
    {
        heartRate = 0;
        rrCount = 0;
        rrIndex = 0;
        derivBaseline = 0;
        hrEnvelope = 18.0f;
        hrFirstValidMs = now;
        hrInstantBpmLp = 0;
        hrInstantInit = false;
    }
}

void sampleAD8232Now()
{
    readAndFilterECG();
    detectHeartRate();
}

// ==========================================
// CẬP NHẬT CHÍNH (gọi trong loop)
// ==========================================
void updateAD8232()
{
    static unsigned long lastSampleTime = 0;
    unsigned long currentTime = micros();

    // Lấy mẫu theo tần số SAMPLE_RATE (250Hz -> 4000us)
    unsigned long sampleInterval = 1000000UL / SAMPLE_RATE;

    if (currentTime - lastSampleTime >= sampleInterval)
    {
        lastSampleTime = currentTime;
        sampleAD8232Now();
    }
}

// ==========================================
// HÀM LẤY DỮ LIỆU
// ==========================================
float getECGRawSignal()
{
    return rawSignal;
}

float getECGFilteredSignal()
{
    return filteredSignal;
}

int getHeartRate()
{
    return heartRate;
}

bool consumeEcgBeatDetected()
{
    const bool detected = beatDetected;
    beatDetected = false;
    return detected;
}

bool areLeadsConnected()
{
    return leadsConnected;
}

bool isAD8232Available()
{
    return adsAvailable;
}

// ==========================================
// IN THÔNG TIN DEBUG (tùy chọn)
// ==========================================
void printECGDebug()
{
    static unsigned long lastPrintTime = 0;
    unsigned long currentTime = millis();

    // In mỗi 500ms
    if (currentTime - lastPrintTime >= 500)
    {
        lastPrintTime = currentTime;

        Serial.println("=== AD8232 ECG Status ===");
        Serial.print("Leads Connected: ");
        Serial.println(leadsConnected ? "YES" : "NO");

        if (leadsConnected)
        {
            Serial.print("Raw Signal: ");
            Serial.print(rawSignal);
            Serial.println(" mV");

            Serial.print("Filtered Signal: ");
            Serial.print(filteredSignal);
            Serial.println(" mV");

            Serial.print("Heart Rate: ");
            Serial.print(heartRate);
            Serial.println(" BPM");
        }
        Serial.println("========================");
    }
}

// ==========================================
// VẼ WAVEFORM LÊN LVGL (nếu cần)
// ==========================================
// Để tích hợp vào UI, bạn có thể tạo một chart LVGL
// và update data points bằng filteredSignal
// Ví dụ:
// lv_chart_set_next_value(chart, ser, (int32_t)filteredSignal);
