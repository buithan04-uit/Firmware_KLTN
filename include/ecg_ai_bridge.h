/*
 * ===================================================================
 * ECG AI PREPROCESSING BRIDGE
 * ===================================================================
 *
 * Mục đích:
 *   Tạo path độc lập từ ADC raw → window 100 mẫu chuẩn MIT-BIH
 *   để gửi lên AI model (CNN 4-class: N/S/V/F).
 *
 * KHÔNG dùng cho display — path hiển thị vẫn dùng filteredSignal
 * từ ad8232.cpp như cũ.
 *
 * Pipeline riêng:
 *   rawADC (250Hz) → HPF → Notch50Hz → AI-LPF (không có spike gate)
 *   → resample 250Hz→360Hz (linear interpolation)
 *   → R-peak detect → extract window [−50..+50] mẫu @ 360Hz
 *   → normalize: (x − MITBIH_MEAN) / MITBIH_STD
 *   → output: float[100] sẵn sàng cho model
 *
 * Cách tích hợp:
 *   1. Gọi EcgAiBridge::pushSample(rawMv) mỗi lần có mẫu mới ở 250Hz
 *      (trong readAndFilterECG(), trước spike gate)
 *   2. Kiểm tra EcgAiBridge::windowReady() trong loop chính
 *   3. Lấy window: EcgAiBridge::getWindow(float out[100])
 *   4. Gửi qua MQTT hoặc HTTP đến AI server
 *
 * Tương thích MIT-BIH:
 *   - MEAN = -0.28766632  STD = 0.52417608  (từ notebook training)
 *   - Window = 100 mẫu, R-peak ở giữa (index 50)
 *   - Sample rate sau resample = 360Hz
 *
 * ===================================================================
 */

#ifndef ECG_AI_BRIDGE_H
#define ECG_AI_BRIDGE_H

#include <Arduino.h>
#include <math.h>

// ==========================================
// THAM SỐ CHUẨN MIT-BIH
// ==========================================
// Lấy từ notebook training: MEAN và STD của tập MIT-BIH sau khi load
// Dùng để normalize: x_norm = (x_mv - MITBIH_MEAN) / MITBIH_STD
// Lưu ý: MIT-BIH lưu tín hiệu ở đơn vị mV (thực), không phải ADC count
static constexpr float MITBIH_MEAN = -0.28766632f;
static constexpr float MITBIH_STD  =  0.52417608f;
static constexpr float ECG_AI_POLARITY = 1.0f; // set to -1.0f if the hardware lead orientation inverts R-peaks

// ==========================================
// ADAPTIVE GAIN (tự calibrate biên độ thiết bị về thang MIT-BIH)
// ==========================================
// Biên độ tín hiệu AC sau HPF/Notch/LPF phụ thuộc gain analog của board
// (AD8232 + ADS1115), vốn không biết chính xác và có thể lệch MIT-BIH
// 100-600x tuỳ phần cứng/điện cực. Thay vì dùng 1 hằng số đoán trước,
// ta theo dõi phương sai (EMA) của tín hiệu AC liên tục và suy ra hệ số
// gain hiện tại = std_device / MITBIH_STD, rồi chia ngược lại trước khi
// normalize. EMA chạy chậm (~vài giây) để không xoá mất sự khác biệt
// biên độ giữa các nhịp (đặc trưng quan trọng để phân loại V/PVC).
static constexpr float AI_GAIN_EMA_ALPHA = 0.0005f; // ~8s time constant @ 250Hz
static constexpr float AI_GAIN_SEED_ESTIMATE = 100.0f; // gain giả định ban đầu trước khi EMA hội tụ
static constexpr float AI_GAIN_MIN = 10.0f;   // chặn dưới, tránh chia gần 0 khi tín hiệu yếu/mất điện cực
static constexpr float AI_GAIN_MAX = 1000.0f; // chặn trên, tránh khuếch đại nhiễu nền thành "tín hiệu"

// ==========================================
// CẤU HÌNH RESAMPLE
// ==========================================
static constexpr float AI_INPUT_FS   = 360.0f; // Hz — tần số MIT-BIH
static constexpr float DEVICE_FS     = 250.0f; // Hz — tần số thiết bị
static constexpr int   AI_WINDOW     = 100;     // số mẫu đầu vào model
static constexpr int   AI_HALF_WIN   = 50;      // mẫu trước/sau R-peak

// ==========================================
// THAM SỐ FILTER RIÊNG CHO AI PATH
// ==========================================
// HPF giống ad8232.cpp (fc ≈ 0.7Hz @ 250Hz)
static constexpr float AI_HPF_ALPHA = 0.983f;

// Notch 50Hz — dùng lại hệ số từ ad8232.cpp
static constexpr float AI_NOTCH_B0 =  0.95459f;
static constexpr float AI_NOTCH_B1 = -0.58970f;
static constexpr float AI_NOTCH_B2 =  0.95459f;
static constexpr float AI_NOTCH_A1 = -0.58970f;
static constexpr float AI_NOTCH_A2 =  0.90919f;

// LPF nhẹ cho AI — cutoff ~40Hz @ 250Hz, giữ toàn bộ QRS morphology
// KHÔNG dùng adaptive LPF, KHÔNG có spike gate → giữ biên độ thật
static constexpr float AI_LPF_ALPHA = 0.90f;

// ==========================================
// THAM SỐ R-PEAK DETECTOR (AI path)
// ==========================================
// Refractory period: 400ms @ 360Hz = 144 mẫu (sau resample)
// → tối đa 150 BPM, an toàn hơn HR_REFRACTORY_MS=450ms của display
static constexpr int   AI_REFRACTORY_SAMPLES = 144; // @ 360Hz
static constexpr float AI_PEAK_ENV_FAST       = 0.10f; // alpha khi vượt envelope
static constexpr float AI_PEAK_ENV_SLOW       = 0.002f; // alpha khi dưới envelope
static constexpr float AI_PEAK_THRESHOLD_RATIO = 0.55f; // ngưỡng = 55% envelope
static constexpr float AI_PEAK_MIN_THRESHOLD   = 8.0f;  // mV tối thiểu (sau HPF)

// ==========================================
// BUFFER SIZES
// ==========================================
// Buffer tín hiệu 250Hz: giữ đủ để resample và detect
// Cần ít nhất: AI_HALF_WIN * (DEVICE_FS/AI_INPUT_FS) * 2 = 100 * 0.694 * 2 ≈ 140 mẫu
// Dùng 300 để có đủ lookback
static constexpr int AI_RAW_BUF_SIZE = 300;

// Buffer sau resample 360Hz: 2 * AI_WINDOW đủ để extract bất kỳ lúc nào
static constexpr int AI_RESAMP_BUF_SIZE = 256; // phải là power of 2 để mask dễ hơn

class EcgAiBridge
{
public:
    EcgAiBridge();

    /**
     * Reset toàn bộ trạng thái (gọi khi leads disconnect)
     */
    void reset();

    /**
     * Nhận 1 mẫu mới từ ADC raw (đơn vị mV, TRƯỚC khi qua spike gate)
     * Gọi trong readAndFilterECG() — xem hướng dẫn tích hợp trong ad8232.cpp
     *
     * @param rawMv  Giá trị tín hiệu raw (adcValue * 0.125f), đã qua step-clamp 300mV
     *               nhưng CHƯA qua LPF adaptive và CHƯA qua spike gate
     */
    void pushSample(float rawMv);

    /**
     * Kiểm tra có window mới sẵn sàng để gửi cho AI không.
     * Trả về true một lần cho mỗi beat được detect.
     * Sau khi gọi getWindow(), cờ này tự reset.
     */
    bool windowReady() const;

    /**
     * Lấy window đã normalize, sẵn sàng đưa vào model.
     * out[100]: float, normalized theo MIT-BIH MEAN/STD
     * Trả về false nếu chưa có window (windowReady() == false)
     */
    bool getWindow(float out[AI_WINDOW]);

    /**
     * Lấy beat index (số beat đã detect từ khi reset) — dùng để debug
     */
    uint32_t beatCount() const { return beatCount_; }

private:
    // --- Filter states (250Hz path) ---
    float hpfPrevIn_;
    float hpfPrevOut_;
    float notchX1_, notchX2_, notchY1_, notchY2_;
    float lpfPrev_;

    // --- Adaptive gain estimate (EMA of filtered signal variance) ---
    float varEstimate_;

    // --- Raw buffer @250Hz (ring buffer) ---
    float rawBuf_[AI_RAW_BUF_SIZE];
    int   rawHead_;      // index mẫu tiếp theo sẽ ghi
    int   rawCount_;     // số mẫu hợp lệ trong buffer

    // --- Resample state ---
    // Dùng linear interpolation: theo dõi phase tích lũy
    int   resampAccum_;  // rational 250Hz -> 360Hz accumulator
    float resampPrev_;   // previous 250Hz sample for interpolation

    // --- Resampled buffer @360Hz (ring buffer) ---
    float rsmpBuf_[AI_RESAMP_BUF_SIZE];
    int   rsmpHead_;
    int   rsmpCount_;    // tổng số mẫu đã push vào resampled buffer

    // --- R-peak detector (@360Hz, chạy trên rsmpBuf_) ---
    float peakEnv_;
    float peakPrev1_;    // mẫu [n-1]
    float peakPrev2_;    // mẫu [n-2]
    int   refractoryCnt_;
    bool  peakInitDone_;
    int   pendingPeakIdx_;
    int   pendingPostSamples_;

    // --- Window state ---
    float  window_[AI_WINDOW]; // window đã chuẩn hóa, sẵn sàng gửi
    bool   windowReady_;
    uint32_t beatCount_;

    // --- Internal helpers ---
    float applyHPF(float in);
    float applyNotch(float in);
    float applyLPF(float in);
    void  pushResampled(float mv250Sample);
    void  runPeakDetector(float newSample360);
    bool  extractWindow(int rPeakIdx360); // rPeakIdx360 = vị trí trong rsmpBuf_
};

// ==========================================
// SINGLETON INSTANCE (dùng trong ad8232.cpp)
// ==========================================
extern EcgAiBridge ecgAiBridge;

#endif // ECG_AI_BRIDGE_H
