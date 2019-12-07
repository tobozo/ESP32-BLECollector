/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  Source: https://github.com/tobozo/ESP32-BLECollector

  MIT License

  Copyright (c) 2018 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

  Hardware requirements:
    - [mandatory] ESP32 (PSRam optional but recommended)
    - [mandatory] SD Card breakout (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - [mandatory] Micro SD (FAT32 formatted, max 32GB)
    - [mandatory] ST7789/ILI9341 320x240 TFT (or bundled in Wrover-Kit, Odroid-Go, M5Stack, LoLinD32 Pro)
    - [mandatory] 'mac-oui-light.db' and 'ble-oui.db' files copied on the Micro SD Card root (optional if you have another running BLECollector)
    - [optional] I2C RTC Module (see "#define HAS_EXTERNAL_RTC" in settings.h or display config)
    - [optional] Serial GPS Module (see "#define HAS_GPS" in settings.h or display config)

  Arduino IDE Settings:
    - Partition Scheme : Minimal SPIFFS (Large APPS with OTA)

  Optional I2C RTC Module requirements:
    - Flash the sketch to setup time in RTC

  Optional I2C+GPS:
    - Flash the sketch, wait for a GPS fix, then issue the "gpstime" command

*/

#include "Settings.h"

void setup() {
  BLECollector.init();
}


void loop() {
  vTaskSuspend(NULL);
}
