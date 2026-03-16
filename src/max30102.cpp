#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

// --- CẤU HÌNH HỆ THỐNG ---
#define MAX_BRIGHTNESS 255
#define BUFFER_SIZE 100        // Độ dài bộ đệm (100 mẫu = 1 giây ở 100Hz)
#define SAMPLES_TO_READ 25     // Số mẫu đọc mới mỗi vòng lặp (cập nhật 4 lần/giây)
#define FINGER_THRESHOLD 50000 // Ngưỡng phát hiện tay (IR > 50k là có tay)

// Biến lưu dữ liệu thô (32-bit cho ESP32)
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

// Biến kết quả từ thư viện Maxim (chỉ dùng HeartRate)
int32_t maxim_spo2;
int8_t validSPO2;
int32_t maxim_heartRate;
int8_t validHeartRate;

// Biến cho thuật toán RMS SpO2 (Mượt hơn)
double rms_SpO2 = 0; // Giá trị SpO2 hiện tại
double fSpO2 = 0.7;  // Hệ số lọc (0.7 giữ cũ, 0.3 cập nhật mới -> Giúp số không bị nhảy)

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22); // Chân SDA, SCL của ESP32

    // Khởi động cảm biến với tốc độ cao
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
    {
        Serial.println(F("MAX30102 not found. Check wiring (SDA=21, SCL=22)!"));
        while (1)
            ;
    }

    // --- CẤU HÌNH CẢM BIẾN (TỐI ƯU CHO ESP32 & ĐỘ NHẠY CAO) ---
    byte ledBrightness = 60; // 0x3C: Độ sáng vừa phải (Tránh bão hòa)
    byte sampleAverage = 4;  // Trung bình 4 mẫu (Giảm nhiễu phần cứng)
    byte ledMode = 2;        // Red + IR
    int sampleRate = 100;    // 100Hz (Bắt buộc để khớp với thuật toán Maxim)
    int pulseWidth = 411;    // 411us (Độ rộng xung lớn nhất -> Nhận nhiều ánh sáng nhất)
    int adcRange = 4096;     // Dải đo 4096 (Cân bằng tốt nhất)

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

    Serial.println(F("System Ready. Hay dat tay vao cam bien..."));
}

// --- HÀM TÍNH SpO2 THEO PHƯƠNG PHÁP RMS (Chính xác & Mượt) ---
double calculate_RMS_SpO2(uint32_t *redBuf, uint32_t *irBuf, int bufferLength)
{
    double sumRedRMS = 0.0;
    double sumIRRMS = 0.0;
    double aveRed = 0.0;
    double aveIR = 0.0;

    // 1. Tính giá trị trung bình (DC Component - Nền)
    for (int i = 0; i < bufferLength; i++)
    {
        aveRed += redBuf[i];
        aveIR += irBuf[i];
    }
    aveRed /= bufferLength;
    aveIR /= bufferLength;

    // 2. Tính RMS (AC Component - Tín hiệu tim)
    for (int i = 0; i < bufferLength; i++)
    {
        sumRedRMS += ((redBuf[i] - aveRed) * (redBuf[i] - aveRed));
        sumIRRMS += ((irBuf[i] - aveIR) * (irBuf[i] - aveIR));
    }

    // 3. Tính tỷ số R (Ratio of Ratios)
    double R = (sqrt(sumRedRMS) / aveRed) / (sqrt(sumIRRMS) / aveIR);

    // 4. Công thức tính SpO2 thực nghiệm (Linear Approximation)
    // -23.3 * (R - 0.4) + 100 là công thức chuẩn cho MAX30102
    double spo2_result = -23.3 * (R - 0.4) + 100;

    return spo2_result;
}

void loop()
{
    // --- GIAI ĐOẠN 1: THU THẬP MẪU ĐẦU TIÊN (Khi vừa đặt tay) ---
    // Đoạn này lấp đầy bộ nhớ đệm trước khi tính toán
    for (byte i = 0; i < BUFFER_SIZE; i++)
    {
        while (particleSensor.available() == false)
            particleSensor.check(); // Chờ dữ liệu mới

        redBuffer[i] = particleSensor.getFIFORed();
        irBuffer[i] = particleSensor.getFIFOIR();
        particleSensor.nextSample();
    }

    // Chạy thuật toán lần đầu để khởi tạo các biến
    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &maxim_spo2, &validSPO2, &maxim_heartRate, &validHeartRate);

    // --- GIAI ĐOẠN 2: VÒNG LẶP LIÊN TỤC (REAL-TIME) ---
    while (1)
    {
        // 1. Dịch chuyển bộ nhớ (Cuốn chiếu)
        // Bỏ 25 mẫu cũ nhất, dồn 75 mẫu mới lên đầu
        memmove(redBuffer, &redBuffer[SAMPLES_TO_READ], (BUFFER_SIZE - SAMPLES_TO_READ) * sizeof(uint32_t));
        memmove(irBuffer, &irBuffer[SAMPLES_TO_READ], (BUFFER_SIZE - SAMPLES_TO_READ) * sizeof(uint32_t));

        // 2. Đọc 25 mẫu mới từ cảm biến
        for (byte i = (BUFFER_SIZE - SAMPLES_TO_READ); i < BUFFER_SIZE; i++)
        {
            while (particleSensor.available() == false)
                particleSensor.check();

            redBuffer[i] = particleSensor.getFIFORed();
            irBuffer[i] = particleSensor.getFIFOIR();
            particleSensor.nextSample();

            // 3. KIỂM TRA RÚT TAY (QUAN TRỌNG)
            if (irBuffer[i] < FINGER_THRESHOLD)
            {
                Serial.println("No Finger (Da rut tay)");
                maxim_heartRate = 0;
                rms_SpO2 = 0;
                // Nếu rút tay, thoát vòng lặp while(1) để quay lại setup buffer từ đầu
                goto RESET_SENSOR;
            }
        }

        // 4. TÍNH BPM (Dùng thuật toán Maxim)
        // Thuật toán này đếm đỉnh xung tim cực tốt
        maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &maxim_spo2, &validSPO2, &maxim_heartRate, &validHeartRate);

        // 5. TÍNH SpO2 (Dùng thuật toán RMS tự viết)
        // Thuật toán này tính năng lượng tín hiệu nên SpO2 rất mượt
        double current_rms_spo2 = calculate_RMS_SpO2(redBuffer, irBuffer, BUFFER_SIZE);

        // 6. BỘ LỌC LÀM MƯỢT (Low Pass Filter)
        // Giúp số không bị nhảy loạn xạ
        if (current_rms_spo2 > 100)
            current_rms_spo2 = 99.9; // Chặn trên
        if (current_rms_spo2 < 70)
            current_rms_spo2 = 70; // Chặn dưới

        // Nếu mới đo lần đầu (rms_SpO2 = 0) thì lấy luôn giá trị mới
        if (rms_SpO2 == 0)
            rms_SpO2 = current_rms_spo2;
        else
            rms_SpO2 = (fSpO2 * rms_SpO2) + ((1.0 - fSpO2) * current_rms_spo2);

        // 7. LOGIC HIỂN THỊ THÔNG MINH (CHỈ HIỆN KHI ỔN ĐỊNH)
        // Điều kiện: Có nhịp tim + BPM trong khoảng con người (40-180) + SpO2 > 90%
        if (validHeartRate && maxim_heartRate > 40 && maxim_heartRate < 180 && rms_SpO2 > 90.0)
        {
            Serial.print("Tim: ");
            Serial.print(maxim_heartRate);
            Serial.print(" bpm \t Oxy: ");
            Serial.print(rms_SpO2, 2); // Làm tròn  số lẻ (98.5%)
            Serial.println(" %");
        }
        else
        {
            // Đang tính toán hoặc dữ liệu chưa ổn định
            Serial.print("Dang do... (Signal: ");
            Serial.print(irBuffer[BUFFER_SIZE - 1]); // In tín hiệu IR để check xem tay đặt chuẩn chưa
            Serial.println(")");
        }
    }

RESET_SENSOR:
    // Nếu rút tay ra, code sẽ nhảy về đây và chạy lại vòng for đầu tiên
    delay(500);
}