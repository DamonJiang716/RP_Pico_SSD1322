# Medusa Firmware

RP2040 firmware for the AIMS Medusa eDNA sampler.

The firmware is being developed around a 256×64 SSD1322 OLED display and provides the display, graphics, user input, and hardware interface layers required for the Medusa control system.

> This project is currently under active development.

## Current Status

Implemented so far:

- SSD1322 OLED driver using the Pico SDK
- 256×64 4-bit grayscale framebuffer
- Basic graphics and 8×8 text rendering
- 2-bit grayscale sprite support
- Image-to-sprite conversion tool
- Rotary switch and push-button input handling
- Initial Settings UI prototype
- Power supply and hardware interface testing

Further work will include the remaining operating modes, battery voltage monitoring, RTC integration, and pump control.

## Project Structure

```text
medusa_firmware/
├── assets/
│   └── source/          # Source images for UI graphics
├── docs/                # Development notes and workflows
├── include/             # Header files
│   └── generated/       # Generated sprite assets
├── src/                 # Firmware source and hardware tests
├── tools/               # Image conversion and utility scripts
├── CMakeLists.txt
└── pico_sdk_import.cmake
