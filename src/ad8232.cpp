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

// High-pass filter loại bỏ DC offset & baseline drift
#define HPF_ALPHA 0.983f // fc ≈ 0.7Hz tại 250Hz, giảm baseline wander tốt hơn

// Smart LPF: mượt khi nền phẳng, giảm lọc khi qua QRS để giữ chi tiết
#define LPF_ALPHA_QUIET 0.90f
#define LPF_ALPHA_QRS 0.55f
#define QRS_SLOPE_THRESHOLD_MV 22.0f
#define DISPLAY_BASELINE_ALPHA 0.997f

// 50Hz notch @ Fs=250Hz (biquad, Q~10) để giảm nhiễu điện lưới
#define NOTCH_B0 0.95459f
#define NOTCH_B1 -0.58970f
#define NOTCH_B2 0.95459f
#define NOTCH_A1 -0.58970f
#define NOTCH_A2 0.90919f

// Nhánh HR riêng: lọc nhẹ để giảm false peak nhưng vẫn giữ QRS
#define HR_LPF_ALPHA 0.72f

// ==========================================
// PHÁT HIỆN NHỊP TIM (R-R Interval)
// ==========================================
#define HR_REFRACTORY_MS 430 // Min 430ms giữa 2 nhịp (~139 BPM max), giảm double count
#define HR_TIMEOUT_MS 4500   // Timeout 4.5s để tránh mất BPM khi tín hiệu yếu ngắn hạn
#define HR_MIN_BPM 45
#define HR_MAX_BPM 130
#define HR_STARTUP_BLANK_MS 850
#define HR_RR_BUFFER 8 // Số R-R intervals để tính trung bình
#define HR_BPM_LP_ALPHA 0.68f

// ==========================================
// BIẾN TOÀN CỤC
// ==========================================
Adafruit_ADS1115 ads; // ADS1115 ADC
static TwoWire adsI2C(1);

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
float displayBaseline = 0;
bool adsAvailable = false;

// ==========================================
// HÀM KHỞI TẠO
// ==========================================
void initAD8232()
{
    Serial.println("=== Khởi tạo AD8232 với ADS1115 ===");
    pinMode(LO_PLUS_PIN, INPUT_PULLDOWN);
    pinMode(LO_MINUS_PIN, INPUT_PULLDOWN);

    // 400kHz là tốc độ chuẩn Fast-mode I2C, ADS1115 hỗ trợ tối đa 400kHz
    Wire1.begin(I2C_SDA, I2C_SCL);
    Wire1.setClock(400000);
    delay(50);

    // Retry 3 lần để đảm bảo ADS1115 kịp khởi động (một số board cần thêm thời gian)
    bool found = false;
    for (int attempt = 1; attempt <= 3 && !found; attempt++)
    {
        found = ads.begin(0x48, &Wire1);
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

    // Cấu hình ADS1115
    // Dùng GAIN_ONE để bao trọn dải 0-3.3V của AD8232, tránh clipping đỉnh R
    ads.setGain(GAIN_ONE);                // ±4.096V range
    ads.setDataRate(RATE_ADS1115_475SPS); // 475 SPS ổn định nhiễu hơn cho ECG

    Serial.println("⚙️  ADS1115 Config:");
    Serial.println("   - Gain: ±4.096V (0.125 mV/bit)");
    Serial.println("   - Rate: 475 SPS");

    Serial.println("✓ ADS1115 khởi tạo thành công");

    // TEST ADS1115 - Đọc tất cả các kênh
    Serial.println();
    Serial.println("🔍 TESTING ADS1115 - Đọc tất cả kênh:");
    delay(100);

    int16_t adc0 = ads.readADC_SingleEnded(0);
    delay(10);
    int16_t adc1 = ads.readADC_SingleEnded(1);
    delay(10);
    int16_t adc2 = ads.readADC_SingleEnded(2);
    delay(10);
    int16_t adc3 = ads.readADC_SingleEnded(3);

    Serial.print("   A0: ");
    Serial.print(adc0);
    Serial.print(" (");
    Serial.print(adc0 * ADC_MV_PER_BIT);
    Serial.println(" mV)");
    Serial.print("   A1: ");
    Serial.print(adc1);
    Serial.print(" (");
    Serial.print(adc1 * ADC_MV_PER_BIT);
    Serial.println(" mV)");
    Serial.print("   A2: ");
    Serial.print(adc2);
    Serial.print(" (");
    Serial.print(adc2 * ADC_MV_PER_BIT);
    Serial.println(" mV)");
    Serial.print("   A3: ");
    Serial.print(adc3);
    Serial.print(" (");
    Serial.print(adc3 * ADC_MV_PER_BIT);
    Serial.println(" mV)");

    Serial.println();
    Serial.println("⚠️  Nếu A0 = 0 nhưng OUTPUT AD8232 ~1.5V:");
    Serial.println("   1. Thử nối OUTPUT vào A1/A2/A3 xem kênh nào hoạt động");
    Serial.println("   2. Hoặc ADS1115 kênh A0 bị hỏng");
    Serial.println();

    // Pre-fill filter buffers với giá trị ADC đầu tiên
    // Tránh HPF transient khi buffer chuyển từ 0 → 1650mV
    float initMV = adc0 * ADC_MV_PER_BIT;
    if (adc0 <= 0 || adc0 >= ADC_MAX_VALID)
        initMV = 1650.0; // Fallback nếu đọc lỗi

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
    displayBaseline = 0;
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
float applyLowPassFilter(float input)
{
    static float prevInput = 0;
    float slope = fabsf(input - prevInput);
    prevInput = input;

    float alpha = (slope > QRS_SLOPE_THRESHOLD_MV) ? LPF_ALPHA_QRS : LPF_ALPHA_QUIET;
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
    static int16_t lastGoodADCValue = 13200; // ~1650mV baseline at GAIN_ONE
    static float lastRawMv = 1650.0f;
    static uint8_t invalidStreak = 0;
    static uint8_t hardFailStreak = 0;
    static unsigned long readBackoffUntilMs = 0;
    static unsigned long lastBusRecoveryAtMs = 0;

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
        // Nếu dây bị tuột, reset các giá trị
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

        // Cảnh báo (mỗi 2s để tránh spam)
        static unsigned long lastWarning = 0;
        if (millis() - lastWarning >= 5000)
        {
            lastWarning = millis();
            Serial.println("[ECG] LEADS DISCONNECTED");
        }
        return;
    }

    int16_t adcValue = lastGoodADCValue;

    // Back off briefly after bursts of invalid reads to avoid flooding Wire errors.
    if (millis() >= readBackoffUntilMs)
    {
        adcValue = ads.readADC_SingleEnded(0);
    }

    // Validate: AD8232 OUTPUT ~1.5V → ADC phải > 0 và < 32767
    // Nếu trả về 0 hoặc 32767 → lỗi I2C/overflow, dùng giá trị tốt cuối cùng
    if (adcValue >= ADC_MIN_VALID && adcValue < ADC_MAX_VALID)
    {
        lastGoodADCValue = adcValue;
        invalidStreak = 0;
        hardFailStreak = 0;
    }
    else
    {
        adcValue = lastGoodADCValue; // Dùng giá trị tốt gần nhất
        if (invalidStreak < 255)
            invalidStreak++;
        if (hardFailStreak < 255)
            hardFailStreak++;

        if (invalidStreak >= 3)
        {
            readBackoffUntilMs = millis() + 30;
            invalidStreak = 0;
        }

        // Recover ADS bus if errors persist for long bursts.
        if (hardFailStreak >= 25 && (millis() - lastBusRecoveryAtMs) >= 1200)
        {
            lastBusRecoveryAtMs = millis();
            Wire1.begin(I2C_SDA, I2C_SCL);
            Wire1.setClock(400000);
            if (ads.begin(0x48, &Wire1)) // Đổi adsI2C thành Wire1
            {
                ads.setGain(GAIN_ONE);
                ads.setDataRate(RATE_ADS1115_475SPS);
            }
            hardFailStreak = 0;
        }
    }

    // Suppress impossible one-sample jumps (typically I2C/ADC glitches).
    float rawMv = adcValue * ADC_MV_PER_BIT;
    float rawStep = rawMv - lastRawMv;
    const float maxRawStepMv = 240.0f;
    if (rawStep > maxRawStepMv)
    {
        rawMv = lastRawMv + maxRawStepMv;
    }
    else if (rawStep < -maxRawStepMv)
    {
        rawMv = lastRawMv - maxRawStepMv;
    }
    lastRawMv = rawMv;
    rawSignal = rawMv;

    // Nhánh lọc thông minh:
    // - notch 50Hz để triệt nhiễu điện lưới
    // - hrInputSignal: lọc nhẹ riêng cho detector
    // - filteredSignal: làm mượt thích nghi cho hiển thị
    // Dùng MA trước median để giảm nhiễu rời rạc và hạn chế peak giả
    float ma = applyMovingAverage(rawSignal);
    float conditioned = applyMedianFilter(ma);
    float notch = applyNotch50Hz(conditioned);
    float ac = applyHighPassFilter(notch);
    hrInputSignal = applyHeartRateLPF(ac);
    filteredSignal = applyLowPassFilter(ac);

    // Loại bỏ trôi nền còn sót ở nhánh hiển thị để waveform ổn định hơn
    displayBaseline = DISPLAY_BASELINE_ALPHA * displayBaseline + (1.0f - DISPLAY_BASELINE_ALPHA) * filteredSignal;
    filteredSignal -= displayBaseline;

    // Soft limiter: tránh clip cứng làm bẹt đỉnh QRS
    float absVal = fabsf(filteredSignal);
    if (absVal > 340.0f)
    {
        float compressed = 340.0f + (absVal - 340.0f) * 0.18f;
        if (compressed > 520.0f)
            compressed = 520.0f;
        filteredSignal = (filteredSignal >= 0.0f) ? compressed : -compressed;
    }

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

    derivBaseline = derivBaseline * 0.996f + absCurr * 0.004f;
    hrEnvelope = hrEnvelope * 0.994f + absCurr * 0.006f;

    float thresholdByDeriv = derivBaseline * 1.75f;
    float thresholdByEnv = hrEnvelope * 1.55f;
    float threshold = fmaxf(thresholdByDeriv, thresholdByEnv);
    if (threshold < 9.0f)
        threshold = 9.0f;
    if (threshold > 95.0f)
        threshold = 95.0f;

    bool localPeak = (hrPrevAbs1 > hrPrevAbs2) && (hrPrevAbs1 > absCurr);
    float prominence = hrPrevAbs1 - fminf(hrPrevAbs2, absCurr);
    float prominenceGate = fmaxf(4.0f, threshold * 0.20f);
    bool prominenceOk = prominence > prominenceGate;
    bool timeOk = (lastRPeakTime == 0) || (now - lastRPeakTime >= HR_REFRACTORY_MS);

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