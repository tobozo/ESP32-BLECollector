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
    - [mandatory] ESP32 (with or without PSRam)
    - [mandatory] SD Card breakout (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - [mandatory] Micro SD (FAT32 formatted, max 32GB)
    - [mandatory] 'mac-oui-light.db' and 'ble-oui.db' files copied on the Micro SD Card root
    - [mandatory] ILI9341 320x240 TFT (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - [optional] I2C RTC Module (see "#define RTC_PROFILE" in settings.h)

  Arduino IDE Settings:
    - Partition Scheme : Minimal SPIFFS (Large APPS with OTA)

  Optional I2C RTC Module requirements:
    - Set "#define RTC_PROFILE NTP_MENU" in settings.h
    - Set your WIFI_SSID and WIFI_PASSWD in settings.h
    - Export compiled binary as "NTPMenu.bin" && copy the file on the SD Card
    - Set "#define RTC_PROFILE CHRONOMANIAC" in settings.h
    - Export compiled binary as "BLEMenu.bin" && copy the file on the SD Card
    - Insert the SD Card
    - Flash the ESP

*/

#include "Settings.h"

void setup() {
  Serial.begin(115200);
  Serial.println(welcomeMessage);
  Serial.println("Free heap at boot: " + String(initial_free_heap));
  BLECollector.init();
}


void loop() {
   #if SCAN_MODE==SCAN_TASK_0 || SCAN_MODE==SCAN_TASK_1 || SCAN_MODE==SCAN_TASK
     vTaskSuspend(NULL);
   #else 
     BLECollector.scanLoop();
   #endif
}
