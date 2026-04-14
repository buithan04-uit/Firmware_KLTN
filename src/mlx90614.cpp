#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// ================================================================
// CẤU HÌNH
// ================================================================
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQ 100000
#define I2C_SCAN_INTERVAL_MS 1000 // Scan I2C lien tuc moi 1 giay
#define MLX90614_ADDR 0x5A
#define MLX_RETRY_INIT_INTERVAL_MS 2000

#define FILTER_SIZE 10      // Lọc 10 mẫu (Độ trễ thấp, phản hồi nhanh)
#define BODY_THRESHOLD 32.0 // Ngưỡng tối thiểu để coi là Da Người

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Biến bộ lọc
float readings[FILTER_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;
bool bufferFilled = false;
unsigned long lastI2CScanMs = 0;
unsigned long lastInitRetryMs = 0;
bool mlxPresent = false;
bool mlxInitialized = false;
uint8_t mlxReadErrorCount = 0;

bool isI2CAddressPresent(uint8_t address)
{
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

void scanI2CDevices()
{
  int found = 0;
  Serial.println("--- I2C SCAN START ---");

  for (uint8_t address = 0x03; address <= 0x77; address++)
  {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("Tim thay I2C device tai 0x");
      if (address < 16)
        Serial.print('0');
      Serial.println(address, HEX);
      found++;
    }
  }

  if (found == 0)
  {
    Serial.println("Khong tim thay I2C device nao");
  }
  else
  {
    Serial.print("Tong so I2C device: ");
    Serial.println(found);
  }

  Serial.println("--- I2C SCAN END ---");
}

void refreshMLXStatus()
{
  bool presentNow = isI2CAddressPresent(MLX90614_ADDR);

  if (presentNow != mlxPresent)
  {
    mlxPresent = presentNow;
    if (mlxPresent)
    {
      Serial.println("[MLX90614] Da phat hien lai tai 0x5A");
    }
    else
    {
      Serial.println("[MLX90614] Mat ket noi I2C (0x5A)");
      mlxInitialized = false;
    }
  }

  if (mlxPresent && !mlxInitialized)
  {
    unsigned long now = millis();
    if (now - lastInitRetryMs >= MLX_RETRY_INIT_INTERVAL_MS)
    {
      lastInitRetryMs = now;
      Serial.println("[MLX90614] Thu khoi tao lai...");
      mlxInitialized = mlx.begin();
      if (mlxInitialized)
      {
        mlxReadErrorCount = 0;
        Serial.println("[MLX90614] Khoi tao thanh cong");
      }
      else
      {
        Serial.println("[MLX90614] Khoi tao that bai");
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);

  delay(300);
  scanI2CDevices();

  mlxPresent = isI2CAddressPresent(MLX90614_ADDR);
  if (mlxPresent)
  {
    mlxInitialized = mlx.begin();
    if (!mlxInitialized)
    {
      Serial.println("Loi khoi tao MLX90614, se tu dong thu lai");
    }
  }
  else
  {
    Serial.println("Chua thay MLX90614 (0x5A), se quet lien tuc");
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
  unsigned long now = millis();
  if (now - lastI2CScanMs >= I2C_SCAN_INTERVAL_MS)
  {
    scanI2CDevices();
    refreshMLXStatus();
    lastI2CScanMs = now;
  }

  if (!mlxPresent || !mlxInitialized)
  {
    delay(100);
    return;
  }

  float rawObj = mlx.readObjectTempC();
  float rawAmb = mlx.readAmbientTempC();

  if (isnan(rawObj) || isnan(rawAmb))
  {
    mlxReadErrorCount++;
    if (mlxReadErrorCount >= 3)
    {
      Serial.println("[MLX90614] Loi doc nhiet do, thu khoi tao lai");
      mlxInitialized = false;
      mlxReadErrorCount = 0;
    }
    delay(100);
    return;
  }

  mlxReadErrorCount = 0;

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