---
name: lcd-lvgl
description: "LCD/LVGL UI conventions: ECG cadence (~8ms), chart SHIFT mode, state stabilization, mutex access patterns, sensor snapshots"
applyTo: "**/lcd*.cpp,**/ui*.cpp,**/ui*.h,src/lcd.cpp"
---

# LCD/LVGL Conventions

## UI Update Cadences

**Critical:** Different screens have different update loops. **ECG must NOT be gated by `SENSOR_UI_UPDATE_MS` (80ms)**.

| Variable                | Purpose                           | Value     | Notes                                                     |
| ----------------------- | --------------------------------- | --------- | --------------------------------------------------------- |
| `ECG_UI_UPDATE_MS`      | ECG waveform chart refresh        | `8` ms    | Real-time, independent of 80ms loop. Use dedicated timer. |
| `ECG_SAMPLE_UPDATE_MS`  | ECG acquisition                   | `4` ms    | Runs in background task on Core 0 at 250 Hz.              |
| `SENSOR_UI_UPDATE_MS`   | HR/SpO2/Temp/Ambient label update | `80` ms   | Shared cadence for non-ECG screens.                       |
| `WIFI_HEADER_UPDATE_MS` | WiFi status icon refresh          | `800` ms  | Low-frequency UI refresh.                                 |
| `CONFIG_UI_UPDATE_MS`   | Config page label sync            | `1000` ms | Web form display.                                         |

**Code pattern** (lcd.cpp, line ~2065):

```cpp
if (in_menu && is_ecg_screen(current_screen_type) && (millis() - lastEcgUiFrameAt >= ECG_UI_UPDATE_MS)) {
    lastEcgUiFrameAt = millis();
    ui_update_ecg_live(filtered, e.heartRateBpm, ecgLive);  // 8ms refresh only
    // ... logging at ECG_LOG_INTERVAL_MS (120ms), not 80ms
}
```

## LVGL Chart Configuration

When updating ECG waveform chart in [ui_ecg.cpp](../src/ui_ecg.cpp):

- **Mode:** Use `LV_CHART_UPDATE_MODE_SHIFT` (not `CIRCULAR` or `CIRCULAR_SHIFT`).
  - `SHIFT` continuously scrolls old data left; no amplitude aliasing.
  - `CIRCULAR` reuses buffer indices; can cause perceived discontinuity.

- **No autoscale:** Set Y-axis range to fixed scale (e.g., 0–4095 for 12-bit ADC or 0–32767 for 16-bit).
  - Dynamic autoscale causes envelope pumping and perceived signal loss.

- **Add period:** When calling `lv_chart_add_series()`, pass full range:
  ```cpp
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4095);  // Fixed for AD8232
  ```

## State Stabilization

When entering sensor screens (Monitor, ECG, SpO2), debounce raw sensor state before notifying UI:

```cpp
// In lcd.cpp lines ~1600+:
StateTracker<MaxRuntimeState> maxStateTracker{MaxRuntimeState::NoSensor, ...};
if (update_stable_state(maxStateTracker, rawMaxState)) {
    // State changed and confirmed >= STATE_CONFIRM_TICKS (3) times.
    notify_max_state(maxStateTracker.stable, ...);
}
```

**Why:** Sensor state flickers (e.g., no-sensor → waiting → live → waiting → live). Require `STATE_CONFIRM_TICKS=3` consecutive reads of same state before UI reacts.

## Mutex/PortMUX Patterns

I2C bus (GPIO 21/22) is shared across multiple tasks. Use semaphore to serialize access:

```cpp
// Global (lcd.cpp, ~line 67):
SemaphoreHandle_t i2c0Mutex = NULL;

// In guiTask setup (lcd.cpp, ~line 1210):
i2c0Mutex = xSemaphoreCreateMutex();

// When reading sensor (any sensor routine):
if (xSemaphoreTake(i2c0Mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    sensorRuntime.updateBackground(true);  // MLX or ADS1115 reads
    xSemaphoreGive(i2c0Mutex);
}
```

**Always use timeout** (e.g., `pdMS_TO_TICKS(50)`); never `portMAX_DELAY`. Prevents UI freeze if I2C hangs.

## Sensor Snapshot Caching

[lcd_sensor_runtime.h](../include/lcd_sensor_runtime.h) caches sensor state to avoid re-reading during UI render:

```cpp
// In lcd.cpp, use snapshot (not live read):
const SensorSnapshot s = sensorRuntime.maxSnapshot();
if (rawMaxState == MaxRuntimeState::LiveData) {
    latestMaxLive = true;
    ui_update_sensors(tempForUi, s.heartRateBpm, s.spo2Percent, s.waveform);
}
```

**Why:** Multiple reads in one UI cycle slow down the loop. Snapshot is updated once per 10ms in `maxSamplingTask`.

## Screen Transition Logic

When switching screens (current_screen_type changes):

1. **Exit old screen:** Clean up state (`collectPendingSend = false`, etc.).
2. **Enter new screen:**
   - If sensor screen (Monitor/ECG/SpO2): Call `sensorRuntime.beginMax30102()` or `beginAd8232()`.
   - If Config screen: Call `wifiConfigManager.forceStartAp()` (non-blocking).
   - If Collect/MeasureAll: Call `collect_ensure_lox_ready()` and reset state.
3. **Refresh headers:** Always call `refresh_wifi_header_ui()` after screen change.

## Ambient Temperature Display

**Convention:** MLX90614 ambient reading feeds both Monitor and Temp screens via shared `ui_set_ambient_temp()` label.

```cpp
// In main loop (lcd.cpp, ~line 2050):
if (sensorRuntime.mlxReady()) {
    ui_set_ambient_temp(sensorRuntime.mlxAmbientTempC());
}
```

Do NOT hardcode ambient values. Always pull from MLX live.

## WiFi Header Async Updates

To avoid UI freeze during WiFi reconnect, **never block in WiFi callbacks**:

```cpp
// WRONG:
void exitConfigMode() {
    WiFi.begin(ssid, pass);  // BLOCKS!
}

// RIGHT:
// In WifiConfigManager::update() (async state machine):
if (reconnectNeeded && (now - lastReconnectAt >= RECONNECT_INTERVAL)) {
    WiFi.begin(...);
    reconnectNeeded = false;
}
```

WiFi state transitions happen in `wifiConfigManager.update()` (async loop), not in screen-exit handlers.

## LVGL Chart Memory

Allocate ECG chart carefully to avoid stack overflow:

- In [include/lv_conf.h](../include/lv_conf.h): Set `LV_MEM_CUSTOM = 1` (use malloc/free, not pool).
- In [platformio.ini](../platformio.ini): Add `-D CONFIG_ARDUINO_LOOP_STACK_SIZE=32768` (32 KB loop stack).
- Pre-allocate chart in `ui_init()` once, do NOT recreate on every frame.

## Logging Cadence

Log critical events at _per-state_, not per-frame:

```cpp
// RIGHT:
if (update_stable_state(stateTracker, rawState)) {  // Only log when state changes
    Serial.println("[ECG] Live signal detected.");
}

// WRONG:
if (ecgLive) {  // Logs every 4ms
    Serial.println("[ECG] Live");
}
```

Use separate `*LiveLog` timers for periodic diagnostic logs:

```cpp
if (ecgLive && (millis() - lastEcgLiveLog >= ECG_LOG_INTERVAL_MS)) {
    lastEcgLiveLog = millis();
    Serial.printf("[ECG] HR=%d, Filtered=%.2f\n", e.heartRateBpm, filtered);
}
```
