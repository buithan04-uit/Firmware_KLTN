/*
 * ===================================================================
 * ECG AI PREPROCESSING BRIDGE — IMPLEMENTATION
 * ===================================================================
 */

#include "ecg_ai_bridge.h"

// Singleton
EcgAiBridge ecgAiBridge;

// ==========================================
// CONSTRUCTOR / RESET
// ==========================================
EcgAiBridge::EcgAiBridge()
{
    reset();
}

void EcgAiBridge::reset()
{
    // Filter states
    hpfPrevIn_ = 0.0f;
    hpfPrevOut_ = 0.0f;
    notchX1_ = notchX2_ = notchY1_ = notchY2_ = 0.0f;
    lpfPrev_ = 0.0f;

    // Raw buffer
    for (int i = 0; i < AI_RAW_BUF_SIZE; i++)
        rawBuf_[i] = 0.0f;
    rawHead_ = 0;
    rawCount_ = 0;

    // Resample
    resampAccum_ = static_cast<int>(DEVICE_FS);
    resampPrev_ = 0.0f;

    // Resampled buffer
    for (int i = 0; i < AI_RESAMP_BUF_SIZE; i++)
        rsmpBuf_[i] = 0.0f;
    rsmpHead_ = 0;
    rsmpCount_ = 0;

    // Peak detector
    peakEnv_ = 20.0f;
    peakPrev1_ = 0.0f;
    peakPrev2_ = 0.0f;
    refractoryCnt_ = 0;
    peakInitDone_ = false;
    pendingPeakIdx_ = -1;
    pendingPostSamples_ = 0;

    // Window
    for (int i = 0; i < AI_WINDOW; i++)
        window_[i] = 0.0f;
    windowReady_ = false;
    beatCount_ = 0;
}

// ==========================================
// FILTER HELPERS (chạy @ 250Hz)
// ==========================================

float EcgAiBridge::applyHPF(float in)
{
    // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    float out = AI_HPF_ALPHA * (hpfPrevOut_ + in - hpfPrevIn_);
    hpfPrevIn_ = in;
    hpfPrevOut_ = out;
    return out;
}

float EcgAiBridge::applyNotch(float in)
{
    float y = AI_NOTCH_B0 * in + AI_NOTCH_B1 * notchX1_ + AI_NOTCH_B2 * notchX2_ - AI_NOTCH_A1 * notchY1_ - AI_NOTCH_A2 * notchY2_;
    notchX2_ = notchX1_;
    notchX1_ = in;
    notchY2_ = notchY1_;
    notchY1_ = y;
    return y;
}

float EcgAiBridge::applyLPF(float in)
{
    // LPF cố định, KHÔNG adaptive, KHÔNG spike gate
    // Mục đích: giảm nhiễu HF nhẹ nhàng, giữ toàn bộ hình dạng QRS
    lpfPrev_ = AI_LPF_ALPHA * lpfPrev_ + (1.0f - AI_LPF_ALPHA) * in;
    return lpfPrev_;
}

// ==========================================
// RESAMPLE 250Hz → 360Hz (linear interpolation)
// ==========================================
// Nguyên lý: mỗi mẫu 250Hz ta tạo ra (AI_INPUT_FS/DEVICE_FS) = 1.44 mẫu 360Hz
// Dùng phase accumulator để quyết định khi nào xuất mẫu mới.
// Với mỗi mẫu 250Hz đến (giá trị = cur), ta nội suy giữa prev và cur.
//
// phase tăng 1.0 mỗi mẫu 250Hz.
// Mỗi khi phase tích lũy đủ (DEVICE_FS/AI_INPUT_FS) = 0.6944 ta xuất 1 mẫu 360Hz.
// Tương đương: ta xuất mẫu 360Hz khi phase vượt mốc kế tiếp cách nhau 0.6944.

void EcgAiBridge::pushResampled(float cur250)
{
    // Stable rational resampler: output samples are spaced by 250/360 of a
    // 250Hz sample interval. The integer phase stays bounded and drift-free.
    if (rawCount_ <= 1)
    {
        resampPrev_ = cur250;
        return;
    }

    while (resampAccum_ <= static_cast<int>(AI_INPUT_FS))
    {
        const float t = static_cast<float>(resampAccum_) / AI_INPUT_FS;
        const float sample360 = resampPrev_ + t * (cur250 - resampPrev_);

        rsmpBuf_[rsmpHead_] = sample360;
        rsmpHead_ = (rsmpHead_ + 1) & (AI_RESAMP_BUF_SIZE - 1);
        rsmpCount_++;

        runPeakDetector(sample360);
        resampAccum_ += static_cast<int>(DEVICE_FS);
    }

    resampAccum_ -= static_cast<int>(AI_INPUT_FS);
    resampPrev_ = cur250;
}

// ==========================================
// R-PEAK DETECTOR (@360Hz)
// ==========================================
// Pan-Tompkins simplified: chạy trên tín hiệu đã qua HPF+Notch+LPF
// Detect local peak dương vượt threshold = AI_PEAK_THRESHOLD_RATIO * envelope
// với refractory period AI_REFRACTORY_SAMPLES mẫu @ 360Hz
//
// Lý do chọn tín hiệu dương: sau HPF, sóng R luôn dương với điện cực đặt đúng
// (Lead II hoặc single-lead tương đương). Không dùng fabsf() để tránh detect sóng T.

void EcgAiBridge::runPeakDetector(float s)
{
    // Complete a pending R-centered window only after AI_HALF_WIN future samples arrive.
    // MIT-BIH training windows are centered on annotated R-peaks, so extracting
    // immediately at peak time would fill the right half with stale ring-buffer data.
    if (pendingPeakIdx_ >= 0)
    {
        pendingPostSamples_++;
        if (pendingPostSamples_ >= AI_HALF_WIN)
        {
            if (extractWindow(pendingPeakIdx_))
            {
                beatCount_++;
                windowReady_ = true;
            }
            pendingPeakIdx_ = -1;
            pendingPostSamples_ = 0;
        }
    }

    float absS = fabsf(s);
    if (absS > peakEnv_)
        peakEnv_ = AI_PEAK_ENV_FAST * absS + (1.0f - AI_PEAK_ENV_FAST) * peakEnv_;
    else
        peakEnv_ = AI_PEAK_ENV_SLOW * absS + (1.0f - AI_PEAK_ENV_SLOW) * peakEnv_;

    if (peakEnv_ < 10.0f)
        peakEnv_ = 10.0f;

    if (!peakInitDone_)
    {
        if (rsmpCount_ >= AI_REFRACTORY_SAMPLES)
            peakInitDone_ = true;
        peakPrev2_ = peakPrev1_;
        peakPrev1_ = s;
        return;
    }

    if (refractoryCnt_ > 0)
        refractoryCnt_--;

    const bool isLocalPeak = (peakPrev1_ > peakPrev2_) && (peakPrev1_ > s);
    if (isLocalPeak && refractoryCnt_ <= 0 && pendingPeakIdx_ < 0)
    {
        float threshold = AI_PEAK_THRESHOLD_RATIO * peakEnv_;
        if (threshold < AI_PEAK_MIN_THRESHOLD)
            threshold = AI_PEAK_MIN_THRESHOLD;

        if (peakPrev1_ > threshold)
        {
            pendingPeakIdx_ = (rsmpHead_ - 2 + AI_RESAMP_BUF_SIZE) & (AI_RESAMP_BUF_SIZE - 1);
            pendingPostSamples_ = 0;
            refractoryCnt_ = AI_REFRACTORY_SAMPLES;
        }
    }

    peakPrev2_ = peakPrev1_;
    peakPrev1_ = s;
}

// ==========================================
// EXTRACT WINDOW + NORMALIZE
// ==========================================
// Cắt AI_WINDOW mẫu quanh R-peak, normalize theo MIT-BIH MEAN/STD
// peakIdxInRsmpBuf: vị trí đỉnh R trong rsmpBuf_ (ring buffer index)
//
// Cần: rsmpCount_ >= AI_HALF_WIN + 2 (đủ mẫu để lấy 50 mẫu trước đỉnh)
// Nếu chưa đủ → bỏ qua beat đầu tiên, không extract

bool EcgAiBridge::extractWindow(int peakIdx)
{
    // Kiểm tra đủ mẫu: cần AI_HALF_WIN mẫu trước và sau
    // rsmpBuf_ là ring buffer, rsmpCount_ tổng mẫu đã đẩy vào
    if (rsmpCount_ < AI_WINDOW)
    {
        return false; // Buffer chưa đủ
    }

    // Tính index bắt đầu (start = peakIdx - AI_HALF_WIN)
    int startIdx = (peakIdx - AI_HALF_WIN + AI_RESAMP_BUF_SIZE) & (AI_RESAMP_BUF_SIZE - 1);

    // Extract AI_WINDOW mẫu liên tiếp, normalize từng mẫu
    for (int i = 0; i < AI_WINDOW; i++)
    {
        int idx = (startIdx + i) & (AI_RESAMP_BUF_SIZE - 1);
        float mv = rsmpBuf_[idx];

        // Normalize theo MIT-BIH: z = (x - MEAN) / STD
        // Tín hiệu của bạn sau HPF đã là AC (zero-mean), còn MIT-BIH MEAN≈-0.288
        // Tuy nhiên ta vẫn dùng đúng MEAN/STD từ training để đảm bảo match
        //
        // Lưu ý quan trọng về đơn vị:
        // MIT-BIH: tín hiệu lưu ở đơn vị mV (thực), sau convert từ ADC counts
        // Thiết bị của bạn: filteredSignal sau HPF cũng ở mV (AC)
        // → Đơn vị khớp nhau, chỉ cần shift/scale bằng MEAN/STD
        //
        // MIT-BIH MEAN ≈ -0.288 mV: DC offset nhỏ từ baseline của dataset
        // MIT-BIH STD ≈ 0.524 mV: biên độ RMS điển hình của QRS trong dataset
        // Tín hiệu của bạn sau HPF: biên độ QRS điển hình ~50-300 mV
        //
        // *** CRITICAL: Biên độ tín hiệu của bạn lớn hơn MIT-BIH ~100-600x ***
        // Lý do: AD8232 có gain ~100 bên trong, ADS1115 đo điện áp output (mV),
        // còn MIT-BIH lưu tín hiệu ECG thực ở body surface (~0.1-1 mV).
        //
        // FIX: Chia thêm cho hệ số gain AD8232 (~100) trước khi normalize:
        float mv_body = mv / ECG_ANALOG_GAIN_ESTIMATE;
        window_[i] = (mv_body - MITBIH_MEAN) / MITBIH_STD;

        // Clamp để tránh outlier làm mô hình mất ổn định
        if (window_[i] > 8.0f)
            window_[i] = 8.0f;
        if (window_[i] < -8.0f)
            window_[i] = -8.0f;
    }

    return true;
}

// ==========================================
// PUBLIC API
// ==========================================

void EcgAiBridge::pushSample(float rawMv)
{
    // Filter chain riêng cho AI: HPF → Notch → LPF (không spike gate)
    float hpf = applyHPF(rawMv) * ECG_AI_POLARITY;
    float notch = applyNotch(hpf);
    float filtered = applyLPF(notch);

    // Lưu vào raw buffer 250Hz
    rawBuf_[rawHead_] = filtered;
    rawHead_ = (rawHead_ + 1) % AI_RAW_BUF_SIZE;
    if (rawCount_ < AI_RAW_BUF_SIZE)
        rawCount_++;

    // Resample 250Hz → 360Hz và chạy peak detector
    pushResampled(filtered);
}

bool EcgAiBridge::windowReady() const
{
    return windowReady_;
}

bool EcgAiBridge::getWindow(float out[AI_WINDOW])
{
    if (!windowReady_)
        return false;

    for (int i = 0; i < AI_WINDOW; i++)
        out[i] = window_[i];

    windowReady_ = false; // consume — sẵn sàng cho beat tiếp theo
    return true;
}
