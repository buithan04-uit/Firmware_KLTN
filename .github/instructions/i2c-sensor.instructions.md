---
name: i2c-sensor
description: "I2C/Sensor conventions: addresses (0x57, 0x5A, 0x48-0x4B), quick probe (no full scan), recovery, MLX hysteresis, mutex serialization"
applyTo: "**/sensor*.cpp,**/sensor*.h,src/lcd_sensor_runtime.cpp,src/ad8232.cpp"
---

# I2C/Sensor Conventions

## I2C Bus Configuration

**Primary Bus:** GPIO 21 (SDA) / GPIO 22 (SCL) – 50 kHz clock (long wire tolerance).

**Known Device Addresses:**

| Device   | Address        | Bus  | Purpose                              |
| -------- | -------------- | ---- | ------------------------------------ |
| MAX30102 | 0x57           | I2C0 | Heart rate + SpO2                    |
| MLX90614 | 0x5A           | I2C0 | Infrared thermometer                 |
| ADS1115  | 0x48–0x4B      | I2C0 | 16-bit ADC for ECG (see note below)  |
| VL53L0X  | 0x29 (default) | I2C0 | Laser distance sensor (Collect mode) |

**ECG Secondary Path (Main env only):** GPIO 4 (SDA) / GPIO 5 (SCL) – 100 kHz.

- AD8232 module uses separate ADS1115 on GPIO4/5 for isolation.
- Falls back to GPIO 21/22 if GPIO 4/5 fails (see [src/ad8232.cpp](../src/ad8232.cpp)).

## I2C Scanning – DO NOT DO FULL SCAN

**⚠️ Critical:** Avoid full-range scan (0x03–0x77) on boards with sensor stalls.

### What NOT to do:

```cpp
// WRONG - hangs on some boards:
for (address = 3; address < 120; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) { /* found */ }
}
```

### What TO do:

```cpp
// RIGHT - quick probe of expected addresses only:
uint8_t expectedAddrs[] = {0x48, 0x49, 0x4A, 0x4B, 0x57, 0x5A};
for (uint8_t addr : expectedAddrs) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        Serial.printf("[I2C] Found device at 0x%02X\n", addr);
    }
}
```

**Bootup procedure** (lcd.cpp, ~line 170):

```cpp
Serial.println("\n[I2C SCANNER] Quét I2C bus 0 (SDA: 21, SCL: 22)...");
for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    // ... log if found
}
```

_This is acceptable only at boot; do NOT repeat during runtime._

## ADS1115 Detection & Address Resolution

ADS1115 (ECG ADC) address depends on ADDR pin tie:

| ADDR Tie | Address |
| -------- | ------- |
| GND      | 0x48    |
| VCC      | 0x49    |
| SDA      | 0x4A    |
| SCL      | 0x4B    |

**Probe strategy** (in [src/ad8232.cpp](../src/ad8232.cpp)):

1. Try 0x48 (most common).
2. If not found, try 0x49, 0x4A, 0x4B in sequence.
3. If I2C bus hangs during probe, recover: call `Wire.end()` and `Wire.begin()` again.

**Code pattern:**

```cpp
bool ad8232_init() {
    for (uint8_t addr : {0x48, 0x49, 0x4A, 0x4B}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[AD8232] ADS1115 found at 0x%02X\n", addr);
            adsAddress = addr;
            return true;
        }
    }
    Serial.println("[AD8232] ADS1115 NOT found");
    return false;
}
```

## I2C Mutex Serialization

Since multiple tasks access I2C (main loop + background samplers), use FreeRTOS semaphore:

```cpp
// Global (lcd.cpp, ~line 67):
SemaphoreHandle_t i2c0Mutex = NULL;

// Setup (guiTask, ~line 1210):
i2c0Mutex = xSemaphoreCreateMutex();

// Before any I2C read:
if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    sensorRuntime.updateBackground(true);  // MLX + ADS1115 reads
    xSemaphoreGive(i2c0Mutex);
} else {
    // Timeout - skip this cycle, try next time
}
```

**Rules:**

- **Always use timeout** (not `portMAX_DELAY`). Prevents UI freeze if I2C locks.
- **Short timeout:** 10–50 ms (single sensor read << 1 ms; contention rare).
- **Give immediately after read** (not during processing).

## MLX90614 – Hysteresis & Step Limiting

MLX90614 (infrared temp) can spike or drift. Apply stability filters:

### 1. Per-Sample Step Limiter

Reject readings that jump > threshold from previous:

```cpp
const float MAX_STEP = 2.0f;  // °C per sample
float newTemp = mlx.readObjectTempC();
if (fabsf(newTemp - lastTemp) <= MAX_STEP) {
    lastTemp = newTemp;
} else {
    // Reject outlier, keep previous
}
```

### 2. Hysteresis (Ambiguous Zone)

When crossing a threshold (e.g., 36–37°C), require margin to switch state:

```cpp
#define HYSTERESIS 0.3f
if (temp > 37.0f + HYSTERESIS && !inHighState) {
    inHighState = true;
} else if (temp < 37.0f - HYSTERESIS && inHighState) {
    inHighState = false;
}
```

### 3. Mode Gate

Only use body temp when ambient is in realistic range (20–35°C):

```cpp
float bodyTemp = mlx.readObjectTempC();
float ambTemp = mlx.readAmbientTempC();
if (ambTemp >= 20.0f && ambTemp <= 35.0f) {
    useBodyTemp = true;
} else {
    Serial.println("[MLX] Ambient out of range, body temp unreliable");
    useBodyTemp = false;
}
```

## VL53L0X (Distance Sensor) – I2C Recovery

VL53L0X (laser distance) can lock I2C bus. Recovery strategy:

```cpp
static void collect_recover_i2c() {
    Serial.println("[COLLECT] VL53L0X reset I2C");
    Wire.end();
    delay(50);
    Wire.begin(21, 22);
    Wire.setClock(50000);
    Wire.setTimeOut(500);
    collectLox.begin();
}

static bool collect_ensure_lox_ready() {
    if (collectLoxReady) return true;

    if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bool ok = collectLox.begin();
    xSemaphoreGive(i2c0Mutex);

    if (!ok) {
        Serial.println("[COLLECT] VL53L0X init failed");
        collect_recover_i2c();
    }
    collectLoxReady = ok;
    return ok;
}
```

**Pattern:** If I2C hangs, soft-reset the bus (end/delay/begin) before retrying.

## Sensor Module Interface

All sensor modules inherit from [include/sensor_module.h](../include/sensor_module.h):

```cpp
class SensorModule {
public:
    virtual bool begin() = 0;                      // Init + detect
    virtual void update() = 0;                     // Read live
    virtual SensorSnapshot snapshot() const = 0;   // Get cached state
};
```

**Implementations:**

- [max30102_sensor_module.h](../include/max30102_sensor_module.h) / `.cpp`
- [mlx90614_sensor_module.h](../include/mlx90614_sensor_module.h) / `.cpp`
- [ad8232_sensor_module.h](../include/ad8232_sensor_module.h) / `.cpp`

**Contract:**

- `begin()`: Probe address, init device, return true if detected.
- `update()`: Safely read sensor (with mutex if shared bus).
- `snapshot()`: Return cached `SensorSnapshot` (no I2C read, fast).

Use `update()` + `snapshot()` pattern in loops to avoid repeated I2C traffic.

## Wire Configuration

**Initialization** (lcd.cpp, ~line 165):

```cpp
Wire.begin(21, 22);         // SDA=21, SCL=22
Wire.setClock(50000);       // 50 kHz (long wire tolerance)
Wire.setTimeOut(500);       // ms timeout for stuck buses
```

**For ECG ADC (GPIO 4/5, Main env only):**

```cpp
Wire1.begin(4, 5);
Wire1.setClock(50000);
```

## Sensor Ready State Caching

Avoid repeated `begin()` calls. Cache ready state:

```cpp
// In LcdSensorRuntime:
bool maxReady_ = false;
bool ad8232Ready_ = false;

bool LcdSensorRuntime::beginMax30102() {
    if (maxReady_) return true;  // Already detected
    bool ok = max30102Module_.begin();
    if (ok) maxReady_ = true;
    return ok;
}
```

Once a sensor is detected, do NOT re-probe unless explicitly reset.

## Diagnostic Logging

Log sensor state changes, not every sample:

```cpp
// State transition logging (log once):
if (update_stable_state(maxStateTracker, rawMaxState)) {
    MaxRuntimeState state = maxStateTracker.stable;
    if (state == MaxRuntimeState::LiveData) {
        Serial.println("[MAX30102] Live HR detected");
    } else {
        Serial.println("[MAX30102] Waiting for finger");
    }
}

// Periodic diagnostic logs (every 1.5s):
if (maxStateTracker.stable == MaxRuntimeState::LiveData &&
    (millis() - lastMaxLiveLog >= LIVE_LOG_INTERVAL_MS)) {
    lastMaxLiveLog = millis();
    Serial.printf("[MAX30102] HR=%d bpm, SpO2=%d%%, Wave=%d\n",
                  s.heartRateBpm, s.spo2Percent, s.waveform);
}
```

Avoid spam; only log on state change or at long intervals.
