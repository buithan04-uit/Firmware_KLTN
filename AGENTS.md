# ESP32 Medical IoT - Agent Notes

## Scope

This repo uses multiple PlatformIO environments with different entry points. Select the correct environment in VS Code before building or uploading (see [platformio.ini](platformio.ini)).

## Build and Run

- Prefer VS Code PlatformIO commands (Build/Upload/Monitor). PlatformIO CLI may not be in PATH on this machine.
- Serial monitor: 115200 baud. Upload speed: 921600 (see [platformio.ini](platformio.ini)).

## Entry Points (by env)

- [src/main.cpp](src/main.cpp) - Main integrated flow (ECG + UI).
- [src/lcd.cpp](src/lcd.cpp) - LCD dashboard + sensors orchestrator.
- [src/main_ad8232.cpp](src/main_ad8232.cpp) - ECG-focused flow.
- [src/max30102.cpp](src/max30102.cpp) - HR/SpO2-only flow.
- [src/mlx90614.cpp](src/mlx90614.cpp) - Temperature-only flow.

## Key Modules

- Runtime orchestration: [src/lcd_sensor_runtime.cpp](src/lcd_sensor_runtime.cpp), [include/lcd_sensor_runtime.h](include/lcd_sensor_runtime.h)
- UI presenter: [src/lcd_ui_presenter.cpp](src/lcd_ui_presenter.cpp), [include/lcd_ui_presenter.h](include/lcd_ui_presenter.h)
- LVGL config: [include/lv_conf.h](include/lv_conf.h)

## Hardware/I2C Notes

- Primary I2C bus is GPIO21/22 (50 kHz for long wires).
- ADS1115 addresses can be 0x48-0x4B. Avoid full 0x03-0x77 scans to prevent hangs on some boards; prefer quick probes.

## Known Pitfalls / Conventions

- Large LCD builds need `board_build.partitions = huge_app.csv` in relevant envs; verify in [platformio.ini](platformio.ini).
- ECG UI should update at ~8 ms and not be gated by 80 ms UI cadence; LVGL chart should use SHIFT mode for continuity.
- In LCD envs, `TFT_eSPI tft` must be defined in [src/lcd.cpp](src/lcd.cpp) (not just declared extern).
- Avoid blocking WiFi reconnects when leaving config; keep reconnect in async update loops.
- Two new features are currently UI-only and not wired to full logic yet (per project status).

## Docs to Link (avoid duplicating)

- [README.md](README.md)
- [GPIO_PIN_MAPPING.md](GPIO_PIN_MAPPING.md)
- [ECG_TROUBLESHOOTING_GUIDE.md](ECG_TROUBLESHOOTING_GUIDE.md)
- [FIRMWARE_TONG_HOP_CHI_TIET.md](FIRMWARE_TONG_HOP_CHI_TIET.md)
- [IMPROVED_VERSION_GUIDE.md](IMPROVED_VERSION_GUIDE.md)
