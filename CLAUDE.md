# M5Reader Arduino Project Guidelines

## Build Commands
- **Compile/Upload**: Use Arduino IDE with ESP32 S3 Dev Module board (v3.0.6 or earlier)
- **Settings**: PSRAM: "OPI PSRAM", USB CDC On Boot: "Enabled", Flash Size: "16MB (128Mbit)"
- **Libraries**: Include epdiy.h and M5GFX libraries
- **Test**: Open an example sketch and use Arduino IDE's verify button

## Code Style
- **Indentation**: 2 spaces
- **Naming**: camelCase for variables, PascalCase for classes
- **Constants**: Use static constexpr for constants
- **Comments**: Single-line // for brief comments, /* */ for documentation
- **Header Guards**: Use #pragma once for header files
- **Error Handling**: Use return codes where applicable
- **Initialization**: Initialize all variables with default values
- **Display Setup**: Always check for EPD mode and set rotation if needed
- **Memory Management**: Be mindful of PSRAM usage for EPD buffer
- **Library Usage**: Prefer M5GFX drawing primitives over custom implementations
- Avoid writing superficial comments; instead, write clear and self-explanatory code.
