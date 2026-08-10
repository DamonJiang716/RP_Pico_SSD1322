# RP_Pico_SSD1322

A basic hardware and graphics engine for SSD1322. Requires Pico SDK only (no external dependencies).

Capable of drawing shapes, graphics, and font in two sizes.

Developed by Yandong Jiang during an internship at the [Australian Institute of Marine Science](https://www.aims.gov.au/).

> This repository is still under active development.
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
