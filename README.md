# ESP32-S3 DevKitC-1 Firmware (ST7789 170x320)

Self-contained PlatformIO firmware for ESP32-S3 DevKitC-1 (N16R8) with 1.9" ST7789 IPS display and joystick/buttons UI.

## Features

- Vertical animated launcher menu with exactly two apps:
  - **App Store** (empty placeholder + utility stopwatch)
  - **2D Game** with **20 puzzle levels** and per-level failure/success logic
- Top status bar always visible with battery %, HH:MM time, DD/MM/YYYY date, and power status label
- Theme toggle on joystick LEFT/RIGHT
- Joystick navigation and action mapping implemented
- Exit button returns from any app to launcher menu
- Targeted for ST7789 170x320 with X offset applied for panel memory mapping

## Hardware wiring

### Display (4-wire SPI)
- SCK -> GPIO12
- MOSI -> GPIO11
- CS -> GPIO10
- DC -> GPIO9
- RST -> GPIO14
- BLK -> 3.3V
- VCC -> 3.3V
- GND -> GND

### Joystick 5-way
- UP -> GPIO4
- DOWN -> GPIO5
- LEFT -> GPIO6
- RIGHT -> GPIO7
- MID -> GPIO16
- COM -> GND

### Tactile buttons (firmware defaults)
- Button 1 (cursor toggle): GPIO1
- Button 2 (interact/draw/object): GPIO2
- Button 3 (back): GPIO15
- Button 4 (exit app): GPIO17

> If your PCB uses different GPIOs for tactile buttons, edit pin constants in `src/main.cpp`.

## Controls

### Launcher
- UP / DOWN: menu scroll
- MID: launch selected app
- LEFT / RIGHT: cycle UI themes

### Global
- Button 4: exit current app back to launcher

### App Store placeholder / Stopwatch
- MID: start/stop stopwatch
- UP: reset stopwatch
- DOWN: offset cycle
- LEFT/RIGHT: theme toggle

### 2D Game
- LEFT/RIGHT: movement
- MID: jump
- Button 1: cursor mode for draw/select levels
- Button 2: interact
- Button 3: restart level (where applicable)
- Button 4: exit to launcher

## Build

```bash
pio run
```

Build output includes `.bin` files under `.pio/build/esp32-s3-devkitc-1/`.

## Flash

See `FLASH_INSTRUCTIONS.txt`.

## CI

GitHub Actions workflow is included to build firmware and upload `.bin` artifacts on each push/PR.
