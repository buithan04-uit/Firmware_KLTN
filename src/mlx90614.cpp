#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// ================================================================
// CẤU HÌNH
// ================================================================
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQ 100000

#define FILTER_SIZE 10      // Lọc 10 mẫu (Độ trễ thấp, phản hồi nhanh)
#define BODY_THRESHOLD 32.0 // Ngưỡng tối thiểu để coi là Da Người

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Biến bộ lọc
float readings[FILTER_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;
bool bufferFilled = false;

void setup()
{
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);

  if (!mlx.begin())
  {
    Serial.println("Loi ket noi MLX90614");
    while (1)
      ;
  }
  Serial.println("--- HE THONG DO NHIET DO CHINH XAC ---");
  // Reset bộ lọc
  for (int i = 0; i < FILTER_SIZE; i++)
    readings[i] = 0;
}

// Hàm tính thân nhiệt (Chỉ áp dụng khi chắc chắn là người)
float calculateBodyTemp(float skinTemp, float ambientTemp)
{
  float bodyTemp = skinTemp;

  // Bù nhiệt độ môi trường
  if (ambientTemp < 25.0)
  {
    bodyTemp += (25.0 - ambientTemp) * 0.1;
  }

  // Công thức y tế chuẩn (Đã hiệu chỉnh)
  if (bodyTemp < 32.0)
    return bodyTemp; // Giữ nguyên nếu quá thấp
  if (bodyTemp < 34.0)
    return bodyTemp + 3.0;
  if (bodyTemp < 35.5)
    return bodyTemp + 2.4;
  if (bodyTemp < 38.0)
    return bodyTemp + 1.4;
  return bodyTemp + 0.5; // Sốt cao bù ít lại
}

void loop()
{
  float rawObj = mlx.readObjectTempC();
  float rawAmb = mlx.readAmbientTempC();

  if (isnan(rawObj))
    return;

  // --- BỘ LỌC TRUNG BÌNH ---
  total = total - readings[readIndex];
  readings[readIndex] = rawObj;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;
  if (readIndex >= FILTER_SIZE)
  {
    readIndex = 0;
    bufferFilled = true;
  }
  average = (bufferFilled) ? (total / FILTER_SIZE) : (total / readIndex);

  // --- LOGIC HIỂN THỊ THÔNG MINH ---

  Serial.print("Moi truong: ");
  Serial.print(rawAmb, 2);
  Serial.print(" | ");

  // TRƯỜNG HỢP 1: ĐO VẬT THỂ / KHÔNG KHÍ (< 32 độ hoặc > 42 độ)
  if (average < BODY_THRESHOLD || average > 42.0)
  {
    Serial.print("[VAT THE] Nhiet do be mat: ");
    Serial.print(average, 1); // Hiển thị nguyên gốc
    Serial.println(" *C");
  }
  // TRƯỜNG HỢP 2: ĐO NGƯỜI (Trong khoảng 32 - 42 độ)
  else
  {
    float bodyTemp = calculateBodyTemp(average, rawAmb);
    Serial.print("[NGUOI] Da: ");
    Serial.print(average, 2);
    Serial.print(" -> CO THE: ");
    Serial.print(bodyTemp, 2);
    Serial.println(" *C");
  }

  delay(200);
}