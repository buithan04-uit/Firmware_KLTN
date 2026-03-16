/*
 * ===================================================================
 * HƯỚNG DẪN TÍCH HỢP UI LVGL MỚI VÀO MAIN.CPP
 * ===================================================================
 *
 * File này chứa code mẫu để thay thế TFT_eSPI bằng LVGL UI mới
 *
 * BƯỚC 1: Thay đổi includes
 * BƯỚC 2: Khởi tạo LVGL thay vì TFT_eSPI
 * BƯỚC 3: Sử dụng UI functions
 * BƯỚC 4: Implement event handlers
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_VL53L0X.h>

// Thêm include UI mới
#include "ui_datacollector.h"

// ==========================================
// LVGL SETUP
// ==========================================
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

TFT_eSPI tft = TFT_eSPI();

// Rename to avoid conflict with ui.cpp
void my_disp_flush_datacollector(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ==========================================
// CẤU HÌNH WIFI & GG SHEETS
// ==========================================
const char *ssid = "C";
const char *password = "18102001";
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbwY5kIR9FeXT1DND8rfqvesckEdptbt2v3hgehC8wbzYHp1_POFO8n5Qf0BMy1asSc/exec";

// ==========================================
// CẤU HÌNH NÚT BẤM
// ==========================================
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_LEFT 25
#define BTN_RIGHT 26
#define BTN_ENTER 27

// ==========================================
// KHỞI TẠO CÁC CẢM BIẾN
// ==========================================
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// ==========================================
// BIẾN TOÀN CỤC
// ==========================================
int currentID = 1;
int sampleCount = 0;
unsigned long lastUpdate = 0;
bool isMeasuring = false;

// ==========================================
// DISTANCE FILTERING & CALIBRATION
// ==========================================
#define MEASUREMENT_SAMPLES 30

// ============================================================
// 1. MEDIAN FILTER 3 MẪU (Chống spike, không gây trễ)
// ============================================================
// Chỉ 3 mẫu: loại bỏ 1 spike mà không gây cross-contamination
// Thời gian trễ chỉ 2 sample = 200ms (ở 100ms/sample)
static float medBuf[3] = {0, 0, 0};
static int medIdx = 0;
static bool medFilled = false;

float median3Filter(float newVal)
{
    medBuf[medIdx] = newVal;
    medIdx = (medIdx + 1) % 3;
    if (!medFilled && medIdx == 0)
        medFilled = true;
    if (!medFilled)
        return newVal;

    float a = medBuf[0], b = medBuf[1], c = medBuf[2];
    // Tìm median của 3 số không cần sort
    if ((a >= b && a <= c) || (a <= b && a >= c))
        return a;
    if ((b >= a && b <= c) || (b <= a && b >= c))
        return b;
    return c;
}

// ============================================================
// 2. BẢNG HIỆU CHUẨN NỘI SUY DÀY ĐẶC
//    (Dense Piecewise Linear Interpolation)
// ============================================================
// VL53L0X offset KHÔNG đồng đều:
//   - Rất gần (<40mm RAW): offset ~24-26mm (phi tuyến, crosstalk cao)
//   - Gần (40-60mm RAW): offset ~20-22mm (chuyển tiếp)
//   - Trung bình (60-120mm RAW): offset ~18-19mm (ổn định)
//   - Xa (>120mm RAW): offset ~15-17mm (sensor chính xác hơn ở xa)
//
// Dữ liệu tham chiếu thực nghiệm: RAW=46 ở ~24-25mm (≈22mm offset)
//
// *** CÁCH HIỆU CHUẨN THÊM ***
// 1. Đặt vật ở khoảng cách biết trước → ghi RAW từ Serial Monitor
// 2. Sửa rawPts[i] tại vị trí tương ứng

#define CAL_POINTS 12
static const float rawPts[CAL_POINTS] = {
    //  Rất gần     Gần        Trung bình           Xa
    36, 46, 54, 64, 76, 87, 100, 116, 132, 152, 178, 229};
static const float actualPts[CAL_POINTS] = {
    10, 20, 28, 38, 50, 62, 76, 92, 108, 128, 153, 205};
// === CÂN BẰNG 80% CODE CŨ (offset 26mm) + 20% MỚI (offset 15mm) ===
// 80%*26 + 20%*15 = 23.8mm ≈ 24mm trung bình
//  RAW=36→10(off=26)   RAW=46→20(off=26*)  RAW=54→28(off=26)
//  RAW=64→38(off=26)   RAW=76→50(off=26)   RAW=87→62(off=25)
//  RAW=100→76(off=24)  RAW=116→92(off=24)  RAW=132→108(off=24)
//  RAW=152→128(off=24) RAW=178→153(off=25) RAW=229→205(off=24)
// * RAW=46 ở 20mm: khớp dữ liệu Serial Monitor thực nghiệm

float interpolateCalibration(float raw)
{
    // Dưới điểm đầu → ngoại suy tuyến tính (giới hạn ≥ 0)
    if (raw <= rawPts[0])
    {
        float slope = (actualPts[1] - actualPts[0]) / (rawPts[1] - rawPts[0]);
        float result = actualPts[0] + slope * (raw - rawPts[0]);
        return (result < 0) ? 0 : result;
    }
    // Trên điểm cuối → ngoại suy tuyến tính
    if (raw >= rawPts[CAL_POINTS - 1])
    {
        float slope = (actualPts[CAL_POINTS - 1] - actualPts[CAL_POINTS - 2]) /
                      (rawPts[CAL_POINTS - 1] - rawPts[CAL_POINTS - 2]);
        return actualPts[CAL_POINTS - 1] + slope * (raw - rawPts[CAL_POINTS - 1]);
    }
    // Nội suy tuyến tính giữa 2 điểm gần nhất
    for (int i = 0; i < CAL_POINTS - 1; i++)
    {
        if (raw <= rawPts[i + 1])
        {
            float t = (raw - rawPts[i]) / (rawPts[i + 1] - rawPts[i]);
            return actualPts[i] + t * (actualPts[i + 1] - actualPts[i]);
        }
    }
    return raw - 24.0f; // fallback: offset 80% hướng cũ
}

// ============================================================
// 3. RATE-ADAPTIVE EMA (Tự điều chỉnh theo tốc độ thay đổi)
// ============================================================
// Di chuyển nhanh → alpha cao (bám sát ngay) → nhạy
// Đứng yên       → alpha thấp (lọc nhiễu)    → ổn định
// Tốt hơn EMA cố định VÀ nhẹ hơn Kalman đầy đủ
#define EMA_ALPHA_MIN 0.15f      // Đứng yên: lọc mạnh
#define EMA_ALPHA_MAX 0.85f      // Di chuyển: bám sát
#define EMA_THRESHOLD 5.0f       // Ngưỡng "di chuyển" (mm)
#define EMA_FAST_THRESHOLD 15.0f // Ngưỡng "di chuyển nhanh"
static float emaValue = 0;
static bool emaInit = false;

float emaFilter(float newVal)
{
    if (!emaInit)
    {
        emaValue = newVal;
        emaInit = true;
        return emaValue;
    }

    float diff = fabsf(newVal - emaValue);
    float alpha;

    if (diff >= EMA_FAST_THRESHOLD)
    {
        alpha = EMA_ALPHA_MAX; // Di chuyển nhanh → bám sát ngay
    }
    else if (diff <= EMA_THRESHOLD)
    {
        alpha = EMA_ALPHA_MIN; // Đứng yên → lọc nhiễạ
    }
    else
    {
        // Chuyển tiếp tuyến tính mượt giữa 2 ngưỡng
        float t = (diff - EMA_THRESHOLD) / (EMA_FAST_THRESHOLD - EMA_THRESHOLD);
        alpha = EMA_ALPHA_MIN + t * (EMA_ALPHA_MAX - EMA_ALPHA_MIN);
    }

    emaValue = alpha * newVal + (1.0f - alpha) * emaValue;
    return emaValue;
}

// ============================================================
// 3. INSERTION SORT (cho mảng nhỏ)
// ============================================================
static void insertionSort(float arr[], int n)
{
    for (int i = 1; i < n; i++)
    {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ============================================================
// 4. ROBUST MEAN (IQR Outlier Rejection + Trimmed Mean)
// ============================================================
float robustMean(float data[], int n)
{
    if (n == 0)
        return 0;
    if (n == 1)
        return data[0];
    if (n == 2)
        return (data[0] + data[1]) / 2.0f;

    float sorted[50];
    int m = (n > 50) ? 50 : n;
    memcpy(sorted, data, m * sizeof(float));
    insertionSort(sorted, m);

    float q1_pos = 0.25f * (m - 1);
    float q3_pos = 0.75f * (m - 1);
    int q1_lo = (int)q1_pos;
    int q3_lo = (int)q3_pos;
    float Q1 = sorted[q1_lo] + (q1_pos - q1_lo) * (sorted[min(q1_lo + 1, m - 1)] - sorted[q1_lo]);
    float Q3 = sorted[q3_lo] + (q3_pos - q3_lo) * (sorted[min(q3_lo + 1, m - 1)] - sorted[q3_lo]);
    float IQR = Q3 - Q1;

    float lower = Q1 - 1.5f * IQR;
    float upper = Q3 + 1.5f * IQR;

    float sum = 0;
    int cnt = 0;
    for (int i = 0; i < m; i++)
    {
        if (sorted[i] >= lower && sorted[i] <= upper)
        {
            sum += sorted[i];
            cnt++;
        }
    }
    if (cnt == 0)
        return (m % 2 == 0) ? (sorted[m / 2 - 1] + sorted[m / 2]) / 2.0f : sorted[m / 2];
    return sum / cnt;
}

// ============================================================
// 6. HÀM TỔNG HỢP
// ============================================================
// Pipeline: RAW → Median3 (chống spike) → Nội suy (hiệu chuẩn)
float getCorrectedDistance(float rawDist)
{
    float clean = median3Filter(rawDist); // Loại bỏ spike đơn lẻ
    return interpolateCalibration(clean); // Nội suy phi tuyến
}

void recoverI2C()
{
    Serial.println("[WARNING] VL53L0X bi treo! Dang Reset I2C...");
    Wire.end();
    delay(50);
    Wire.begin(21, 22);
    Wire.setClock(40000);
    Wire.setTimeOut(500);
    lox.begin();
}

// ==========================================
// EVENT HANDLERS (Required by UI)
// ==========================================
void ui_event_take_measurement(lv_event_t *e)
{
    if (isMeasuring || sampleCount >= 5)
    {
        if (sampleCount >= 5)
        {
            ui_datacollector_show_popup("CANH BAO", "Da du 5 mau!\nChuyen nguoi tiep",
                                        lv_palette_main(LV_PALETTE_RED), false);
            delay(1500);
            ui_datacollector_hide_popup();
        }
        return;
    }

    isMeasuring = true;

    // Show popup
    char sampMsg[32];
    snprintf(sampMsg, sizeof(sampMsg), "Lay %d mau...", MEASUREMENT_SAMPLES);
    ui_datacollector_show_popup("DANG LAY MAU", sampMsg,
                                lv_palette_main(LV_PALETTE_BLUE), true);

    // Thu thập từng mẫu riêng lẻ để xử lý thống kê
    float distSamples[MEASUREMENT_SAMPLES];
    float objSamples[MEASUREMENT_SAMPLES];
    float ambSamples[MEASUREMENT_SAMPLES];
    int count = 0;

    for (int j = 0; j < MEASUREMENT_SAMPLES; j++)
    {
        VL53L0X_RangingMeasurementData_t measure;
        lox.rangingTest(&measure, false);

        if (measure.RangeStatus == 4 || measure.RangeMilliMeter == 8190)
        {
            recoverI2C();
        }
        else
        {
            distSamples[count] = getCorrectedDistance(measure.RangeMilliMeter);
            objSamples[count] = mlx.readObjectTempC();
            ambSamples[count] = mlx.readAmbientTempC();
            count++;
        }

        // Update progress
        ui_datacollector_update_popup_progress((j + 1) * 100 / MEASUREMENT_SAMPLES);
        lv_task_handler();
        delay(50);
    }

    if (count > 0)
    {
        // Robust statistics: IQR outlier rejection + trimmed mean
        float avgD = robustMean(distSamples, count);
        float avgO = robustMean(objSamples, count);
        float avgA = robustMean(ambSamples, count);

        Serial.printf("[STAT] %d/%d valid samples. Dist=%.1f Obj=%.1f Amb=%.1f\n",
                      count, MEASUREMENT_SAMPLES, avgD, avgO, avgA);

        // Show result
        char msg[96];
        snprintf(msg, sizeof(msg), "KC:%.0fmm  Da:%.1fC  MT:%.1fC", avgD, avgO, avgA);
        ui_datacollector_show_popup("KET QUA DO", msg,
                                    lv_palette_main(LV_PALETTE_GREEN), false);
        lv_task_handler();
        delay(1500);

        // Send to Google Sheets
        ui_datacollector_show_popup("GUI DU LIEU", "Dang gui cloud...",
                                    lv_palette_main(LV_PALETTE_BLUE), false);
        lv_task_handler();

        if (WiFi.status() == WL_CONNECTED)
        {
            HTTPClient http;
            String url = GOOGLE_SCRIPT_URL + "?id=" + String(currentID) +
                         "&obj=" + String(avgO, 2) +
                         "&amb=" + String(avgA, 2) +
                         "&dist=" + String(avgD, 1) +
                         "&omron=";

            http.begin(url.c_str());
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            int httpCode = http.GET();
            http.end();

            if (httpCode > 0)
            {
                sampleCount++;
                ui_datacollector_update_progress(sampleCount, 5);
                ui_datacollector_show_popup("THANH CONG!", "Du lieu da gui!",
                                            lv_palette_main(LV_PALETTE_GREEN), false);
            }
            else
            {
                ui_datacollector_show_popup("LOI MANG", "Khong gui duoc!",
                                            lv_palette_main(LV_PALETTE_RED), false);
            }
        }
        else
        {
            ui_datacollector_show_popup("LOI WIFI", "Mat ket noi!",
                                        lv_palette_main(LV_PALETTE_RED), false);
        }
    }
    else
    {
        ui_datacollector_show_popup("LOI CAM BIEN", "Khong doc duoc!",
                                    lv_palette_main(LV_PALETTE_RED), false);
    }

    delay(2000);
    ui_datacollector_hide_popup();
    isMeasuring = false;
}

void ui_event_id_minus(lv_event_t *e)
{
    if (currentID > 1)
    {
        currentID--;
        sampleCount = 0;
        ui_datacollector_update_id(currentID);
        ui_datacollector_update_progress(sampleCount, 5);
        Serial.printf("[ACTION] ID giam: %d\n", currentID);
    }
}

void ui_event_id_plus(lv_event_t *e)
{
    currentID++;
    sampleCount = 0;
    ui_datacollector_update_id(currentID);
    ui_datacollector_update_progress(sampleCount, 5);
    Serial.printf("[ACTION] ID tang: %d\n", currentID);
}

void ui_event_sample_reset(lv_event_t *e)
{
    sampleCount = 0;
    ui_datacollector_update_progress(sampleCount, 5);
    Serial.println("[ACTION] Reset sample count");
}

// ==========================================
// SETUP
// ==========================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n===== BAT DAU KHOI DONG =====");

    // Buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);
    Serial.println("[OK] Buttons configured");

    // TFT Init
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    Serial.println("[OK] TFT initialized");

    // LVGL Init
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush_datacollector;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("[OK] LVGL initialized");

    // WiFi
    Serial.println("[INFO] Connecting WiFi...");
    WiFi.begin(ssid, password);
    int wifiWait = 0;
    while (WiFi.status() != WL_CONNECTED && wifiWait < 20)
    {
        Serial.print(".");
        delay(500);
        wifiWait++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n[OK] WiFi connected!");
    }
    else
    {
        Serial.println("\n[WARNING] WiFi failed!");
    }

    // I2C Sensors
    Serial.println("[INFO] Initializing I2C...");
    Wire.begin(21, 22);
    Wire.setClock(40000);
    Wire.setTimeOut(500);

    bool mlxOK = mlx.begin();
    bool loxOK = lox.begin();

    if (!mlxOK || !loxOK)
    {
        Serial.println("[ERROR] I2C sensors failed!");
        Serial.printf("MLX: %s, VL53L0X: %s\n", mlxOK ? "OK" : "FAIL", loxOK ? "OK" : "FAIL");
    }
    else
    {
        Serial.println("[OK] I2C sensors ready!");
    }

    // Create UI
    ui_datacollector_init();
    ui_datacollector_update_wifi(WiFi.status() == WL_CONNECTED);
    ui_datacollector_update_id(currentID);
    ui_datacollector_update_progress(sampleCount, 5);

    Serial.println("\n===== KHOI DONG THANH CONG! =====\n");
}

// ==========================================
// LOOP
// ==========================================
void loop()
{
    lv_task_handler(); // LVGL task handler - QUAN TRỌNG!

    // Update sensor data - 100ms cho phản hồi nhanh
    if (millis() - lastUpdate > 100)
    {
        VL53L0X_RangingMeasurementData_t measure;
        lox.rangingTest(&measure, false);

        float liveDist = 999;
        if (measure.RangeStatus == 4 || measure.RangeMilliMeter == 8190)
        {
            recoverI2C();
        }
        else
        {
            // Pipeline: RAW → Nội suy hiệu chuẩn → EMA filter → Hiển thị
            float rawMM = (float)measure.RangeMilliMeter;
            float calibrated = getCorrectedDistance(rawMM); // Nội suy tuyến tính
            liveDist = emaFilter(calibrated);               // EMA mượt & nhanh

            // In RAW mỗi 500ms để hiệu chuẩn
            static unsigned long lastLog = 0;
            if (millis() - lastLog > 500)
            {
                Serial.printf("[LIVE] RAW=%d  CAL=%.1f  DISPLAY=%.1f mm\n",
                              measure.RangeMilliMeter, calibrated, liveDist);
                lastLog = millis();
            }
        }

        float liveTemp = mlx.readObjectTempC();
        float liveAmb = mlx.readAmbientTempC();
        ui_datacollector_update_sensors(liveDist, liveTemp, liveAmb);

        lastUpdate = millis();
    }

    // Button handling
    if (digitalRead(BTN_ENTER) == LOW)
    {
        delay(50);
        if (digitalRead(BTN_ENTER) == LOW)
        {
            ui_event_take_measurement(NULL);
            while (digitalRead(BTN_ENTER) == LOW)
                ;
        }
    }

    if (digitalRead(BTN_RIGHT) == LOW)
    {
        delay(50);
        if (digitalRead(BTN_RIGHT) == LOW)
        {
            ui_event_id_plus(NULL);
            while (digitalRead(BTN_RIGHT) == LOW)
                ;
        }
    }

    if (digitalRead(BTN_LEFT) == LOW)
    {
        delay(50);
        if (digitalRead(BTN_LEFT) == LOW)
        {
            ui_event_id_minus(NULL);
            while (digitalRead(BTN_LEFT) == LOW)
                ;
        }
    }

    if (digitalRead(BTN_DOWN) == LOW)
    {
        delay(50);
        if (digitalRead(BTN_DOWN) == LOW)
        {
            ui_event_sample_reset(NULL);
            while (digitalRead(BTN_DOWN) == LOW)
                ;
        }
    }

    delay(5);
}
