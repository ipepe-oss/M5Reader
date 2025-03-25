# M5Reader
M5Reader - M5PaperS3 based ebook reader

## How to build for M5PaperS3
Source: <https://docs.m5stack.com/en/arduino/m5papers3/program>

Key takeaways:
 * Use the ESP32 S3 Dev Module
 * The board management version must be below v3.0.7, otherwise, it will fail to compile properly.
 * Enable PSRAM, and select the mode as OPI PSRAM
 * USB CDC On Boot: "Enabled"
 * Flash Size: "16MB (128Mbit)"
 * PSRAM: "OPI PSRAM"
 * `#include <epdiy.h>`

## Inspiration for this project
 * E-Ink/E-Reader
   * <https://github.com/Dejvino/lilybook>
   * <https://github.com/atomic14/diy-esp32-epub-reader>
   * <https://github.com/Sarah-C/M5Stack_Paper_E-Book_Reader>
 * RSS Reader
   * <https://github.com/htruong/zenreader>
 * Deep Sleep
   * <https://simplyexplained.com/blog/esp32-tips-to-increase-battery-life/>
   * <https://www.youtube.com/watch?v=KQS_xDDWfLw>
   * <https://www.reddit.com/r/esp32/comments/e8n7p1/esp32_tips_to_increase_battery_life_15_weeks_and/>
   * <https://www.youtube.com/watch?v=kgtsSNjnzSI>
