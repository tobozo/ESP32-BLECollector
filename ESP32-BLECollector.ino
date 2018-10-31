/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card

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


  Hardware requirements:
    - ESP32 (with or without PSRam)
    - SD Card breakout (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - ILI9341 320x240 TFT (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
    - I2C RTC Module
    - Micro SD (FAT32 formatted, max 32GB)
    - 'mac-oui-light.db' and 'ble-oui.db' files copied on the Micro SD Card root

  Arduino IDE Settings:
    - Partition Scheme : No OTA (Large APP)


*/

#include "EBC0_Settings.h"


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  int resetReason = (int)rtc_get_reset_reason(0);
  Serial.begin(115200);
  Serial.println("ESP32 BLE Scanner");
  Serial.println("Free heap at boot: " + String(initial_free_heap));
  Serial.println("I2C is at SDA:" + String(SDA) + " / SCL:" + String(SCL));
  Wire.begin(26, 27); // RTC wired to SDA, SCL (26,27 on Wrover Kit)
  RTC.begin();
  if (!RTC.isrunning()) {
    Serial.println("RTC is NOT running, adjusting and restarting...");
    RTC.adjust(DateTime(__DATE__, __TIME__));
    delay(1000);
    ESP.restart(); // TODO: adjust from a BLE characteristic or a GPS
  } else {
    Serial.println("RTC is running :-)");
    RTC_is_running = true;
    updateTimeString();
  }
  if (resetReason == 12) { // SW Reset
    initUI(false);
  } else {
    initUI();
  }
  // sorry this won't work with SPIFFS, too much ram is eaten by BLE functions :-(
  bool sd_mounted = false;
  while ( sd_mounted == false ) {
    //Out.println("Card Mount Failed, will restart");
    if ( SD_MMC.begin() ) {
      sd_mounted = true;
    } else {
      headerStats("Card Mount Failed");
      delay(500);
      headerStats(" ");
      delay(300);
    }
  }

  sqlite3_initialize();
  initial_free_heap = freeheap;
  xTaskCreatePinnedToCore(heapGraph, "HeapGraph", 1000, NULL, 0, NULL, 1); /* last = Task Core */
  BLEDevice::init("");
  //resetDB();
  pruneDB(); // remove unnecessary/redundant entries
  if ( resetReason == 12)  { // =  SW_CPU_RESET
    // CPU was reset by software, do nothing
    headerStats("Heap heap heap...");
    delay(1000);
  } else {
    // initial boot, perform some tests
    testOUI(); // test oui database
    testVendorNames(); // test vendornames database
    showDataSamples(); // print some of the collected values
  }
  /*
    // test hardware scrolling performances
    while(1) {
      for(int i=0;i<160;i++) {
        setupScrollArea(i, i);
        scroll_slow(320-(i*2), 1);
      }
    }
    // test multiline scrolling (buggy)
    while(1){
      Out.println(String(millis()) + " 7a:33:60:9f:30:6c RSSI: -90 VData: 4c0007190102202b990f01000038455070511385375bed94b82f210428 VName: Apple, Inc.  txPow: 0");
      drawRSSI(random(0, 240), random(0, 320), random(-90, -30));
      Out.println();
      delay(1000);
    }
  **/
}


void loop() {
  doBLEScan();
  if (prune_trigger > prune_threshold) {
    pruneDB();
  }
  if ( freeheap + heap_tolerance < min_free_heap ) {
    headerStats("Out of heap..!");
    Serial.println("Heap too low:" + String(freeheap));
    delay(1000);
    ESP.restart();
  }
  if (RTC_is_running) {
    updateTimeString();
  }
  Serial.printf("Cache hits -- Cards:%s Self:%s Oui:%s, Vendor:%s\n", String(BLEDevCacheHit).c_str(), String(SelfCacheHit).c_str(), String(OuiCacheHit).c_str(), String(VendorCacheHit).c_str());
}
