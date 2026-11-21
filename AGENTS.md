# ESP32 Medical IoT - Agent Notes

## Scope

This repo currently has a single PlatformIO environment, `Main`, defined in [platformio.ini](platformio.ini). There is no environment selection needed before building or uploading.

## Build and Run

- Prefer VS Code PlatformIO commands (Build/Upload/Monitor). PlatformIO CLI may not be in PATH on this machine.
- Serial monitor: 115200 baud. Upload speed: 921600 (see [platformio.ini](platformio.ini)).

## Entry Point

- [src/main.cpp](src/main.cpp) - Integrated flow: LVGL UI, ECG, HR/SpO2, temperature, distance, battery, WiFi/MQTT.

## Key Modules

- Runtime orchestration: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [include/lcd_sensor_runtime.h](include/lcd_sensor_runtime.h)
- UI presenter: [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp), [include/lcd_ui_presenter.h](include/lcd_ui_presenter.h)
- ECG backend: [src/ad8232.cpp](src/ad8232.cpp), [include/ad8232.h](include/ad8232.h)
- LVGL config: [include/lv_conf.h](include/lv_conf.h)

## Hardware/I2C Notes

- Primary I2C bus (Wire) is GPIO21/22 (50 kHz) — MAX30102, MLX90614, INA219, VL53L0X.
- ECG I2C bus (Wire1) is GPIO4/5 — ADS1115 only, address 0x48. Do not confuse the two buses.
- Avoid full 0x03-0x77 scans in new code to prevent hangs on some boards; prefer quick probes.

## Known Pitfalls / Conventions

- Build needs `board_build.partitions = huge_app.csv` (see [platformio.ini](platformio.ini)).
- ECG UI should update at ~8 ms and not be gated by 80 ms UI cadence; LVGL chart should use SHIFT mode for continuity.
- `TFT_eSPI tft` must be defined in [src/main.cpp](src/main.cpp) (not just declared extern).
- Avoid blocking WiFi reconnects when leaving config; keep reconnect in async update loops.
- Some features may be UI-only and not wired to full logic yet — check current code before assuming a feature works end to end.

## Docs to Link (avoid duplicating)

- [README.md](README.md) - project overview
- [FIRMWARE_ARCHITECTURE.md](FIRMWARE_ARCHITECTURE.md) - detailed runtime architecture
- [HARDWARE_WIRING.md](HARDWARE_WIRING.md) - GPIO/I2C/wiring reference
- [ECG_GUIDE.md](ECG_GUIDE.md) - ECG usage and troubleshooting
- [PCB_DESIGN_GUIDE.md](PCB_DESIGN_GUIDE.md) - PCB design guidance
- [ROADMAP_AND_TESTING.md](ROADMAP_AND_TESTING.md) - development roadmap and breadboard test checklist
