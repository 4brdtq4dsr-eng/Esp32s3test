# ESP32-S3 ST7789 170x320 Hardware Test Firmware

Standalone firmware for **ESP32-S3 DevKitC-1 (N16R8)** that verifies:
- ST7789 1.9" IPS SPI display (170x320)
- Joystick digital button inputs (active-low)
- Stopwatch UI with start/stop and theme switching

This repository is self-contained and does **not** use external display libraries.

## Features

- ST7789 display driver implemented locally in `src/main.cpp`
- Fixed panel resolution: **170x320**
- Runtime offset preset cycling for panels that require RAM offsets:
  - Preset 0: X=0, Y=0
  - Preset 1: X=35, Y=0 (default)
  - Preset 2: X=0, Y=80
- Boot screen with Russian title: **"ТЕСТ ЭКРАНА"**
- Stopwatch in format `MM:SS.t`, 10 Hz update when running
- Button controls:
  - `MID`: Start/Stop toggle
  - `UP`: Reset to `00:00.0`
  - `LEFT` / `RIGHT`: Toggle theme (white/red)
  - `DOWN`: Cycle display offset preset live and redraw
- Debounced button press events (edge detect, 40 ms)
- Serial logging for every button press/action

## Hardware Wiring (must match firmware)

### Display ST7789 (4-wire SPI, 1.9" 170x320)

| Display pin | ESP32-S3 pin |
|---|---|
| SCL (SCK) | GPIO12 |
| SDA (MOSI) | GPIO11 |
| CS | GPIO10 |
| DC | GPIO9 |
| RES (RST) | GPIO14 |
| VCC | 3.3V |
| GND | GND |
| BLK | 3.3V (always on) |

> BLK is tied directly to 3.3V in this test (no GPIO backlight control).

### Joystick button module (COM to GND, pressed = LOW)

| Joystick pin | ESP32-S3 pin |
|---|---|
| COM | GND |
| UP | GPIO4 |
| DOWN | GPIO5 |
| LEFT | GPIO6 |
| RIGHT | GPIO7 |
| MID (OK) | GPIO16 |

`SET` / `RST` joystick pins are intentionally not used.

## Build Locally (PlatformIO)

### 1) Build

```bash
pio run
```

### 2) Upload via USB (optional local flash)

```bash
pio run -t upload
```

### 3) Serial monitor

```bash
pio device monitor -b 115200
```

## Flash with `esptool.py` from build outputs

After a successful build, files are in:
- `.pio/build/esp32-s3-devkitc-1/bootloader.bin`
- `.pio/build/esp32-s3-devkitc-1/partitions.bin`
- `.pio/build/esp32-s3-devkitc-1/firmware.bin`
- `.pio/build/esp32-s3-devkitc-1/boot_app0.bin` (if present)

Example flash command (adjust serial port):

```bash
esptool.py --chip esp32s3 --port COMx --baud 921600 --before default_reset --after hard_reset write_flash -z \
  0x0 .pio/build/esp32-s3-devkitc-1/bootloader.bin \
  0x8000 .pio/build/esp32-s3-devkitc-1/partitions.bin \
  0xE000 .pio/build/esp32-s3-devkitc-1/boot_app0.bin \
  0x10000 .pio/build/esp32-s3-devkitc-1/firmware.bin
```

If `boot_app0.bin` is absent in your environment, omit the `0xE000` pair.

## GitHub Actions Artifacts

Workflow: `.github/workflows/build.yml`

- Trigger: `workflow_dispatch`
- Builds firmware using PlatformIO
- Uploads artifact bundle containing:
  - `firmware.bin`
  - `bootloader.bin`
  - `partitions.bin`
  - `boot_app0.bin` (if available)
  - `FLASH_INSTRUCTIONS.txt` with exact addresses and command

To use artifacts:
1. Open Actions tab in GitHub.
2. Run **Build ESP32-S3 Firmware** manually.
3. Download `esp32s3-firmware-artifacts`.
4. Follow `FLASH_INSTRUCTIONS.txt` inside the artifact.
