/*
 * ===================================================================
 * AD8232 ECG SENSOR HEADER FILE
 * ===================================================================
 */

#ifndef AD8232_H
#define AD8232_H

#include <Arduino.h>

// ==========================================
// HÀM KHỞI TẠO VÀ CẬP NHẬT
// ==========================================

/**
 * Khởi tạo AD8232 với ADS1115
 * Cấu hình I2C, ADC, và các bộ lọc
 */
void initAD8232();

/**
 * Cập nhật đọc tín hiệu ECG và phát hiện nhịp tim
 * Gọi hàm này trong loop() chính
 */
void updateAD8232();

// ==========================================
// HÀM LẤY DỮ LIỆU
// ==========================================

/**
 * Lấy tín hiệu ECG thô (chưa lọc) từ ADS1115
 * @return Giá trị điện áp (mV)
 */
float getECGRawSignal();

/**
 * Lấy tín hiệu ECG đã qua bộ lọc nhiễu
 * @return Giá trị điện áp đã lọc (mV)
 */
float getECGFilteredSignal();

/**
 * Lấy nhịp tim hiện tại
 * @return Nhịp tim (BPM - beats per minute), 0 nếu không phát hiện
 */
int getHeartRate();

/**
 * Kiểm tra trạng thái kết nối điện cực
 * @return true nếu điện cực được gắn đúng, false nếu bị tuột
 */
bool areLeadsConnected();

// ==========================================
// HÀM DEBUG (TÙY CHỌN)
// ==========================================

/**
 * In thông tin debug ra Serial Monitor
 * Bao gồm: trạng thái leads, raw signal, filtered signal, heart rate
 */
void printECGDebug();

#endif // AD8232_H
