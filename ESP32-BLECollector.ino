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
    - [mandatory] 'mac-oui-light.db' and 'ble-oui.db' files copied on the Micro SD Card root
    - [mandatory] ILI9341 320x240 TFT (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - [optional] I2C RTC Module (see "#define RTC_PROFILE" in settings.h)

  Arduino IDE Settings:
    - Partition Scheme : Minimal SPIFFS (Large APPS with OTA)

  Optional I2C RTC Module requirements:
    - Insert the SD Card
    - Set "#define RTC_PROFILE NTP_MENU" in Settings.h
    - Set your WIFI_SSID and WIFI_PASSWD in Settings.h
    - Flash the sketch, wait for time synch and SD Card replication (will save itself as "NTPMenu.bin")
    - Set "#define RTC_PROFILE CHRONOMANIAC" in settings.h
    - Flash the sketch, wait for SD Card replication (will save itself as "BLEMenu.bin")

*/

#include "Settings.h"

void setup() {
  #ifdef M5STACK
  M5.begin();
  Wire.begin();
  delay(100); // need this to avoid a boot loop
  if (digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }
  #else
  Serial.begin(115200);
  #endif
  Serial.println(welcomeMessage);
  Serial.printf("RTC_PROFILE: %s\nHAS_EXTERNAL_RTC: %s\nHAS_GPS: %s\nTIME_UPDATE_SOURCE: %d\nSKECTH_MODE: %d\n",
    RTC_PROFILE,
    HAS_EXTERNAL_RTC ? "true" : "false",
    HAS_GPS ? "true" : "false",
    TIME_UPDATE_SOURCE,
    SKETCH_MODE
  );
  Serial.println("Free heap at boot: " + String(initial_free_heap));
  BLECollector.init();
}


void loop() {
  vTaskSuspend(NULL);
}
