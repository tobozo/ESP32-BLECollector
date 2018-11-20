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

  RTC Profiles: 
    1 - "Hobo": No TinyRTC module in your build, only uptime will be displayed
    2 - "Rogue": TinyRTC module adjusted after flashing, no WiFi, no NTP Sync
    3 - "Chronomaniac": TinyRTC module adjusts itself via NTP (separate binary)
   
    Only profile 3 - "Chronomaniac" requires to compile this module in two 
    different modes, see "#define BUILD_NTPMENU_BIN"
 
*/

#define HOBO 1 // No TinyRTC module in your build, only uptime will be displayed
#define ROGUE 2 // TinyRTC module adjusted after flashing, no WiFi, no NTP Sync
#define CHRONOMANIAC 3 // TinyRTC module adjusts itself via NTP (by sd-loading a separate binary, see NTP_MENU)
#define NTP_MENU 4 // use this to produce the NTPMenu.bin, only if you have a RTC module !!

// edit this value to fit your mode
#define RTC_PROFILE HOBO
//#define RTC_PROFILE NTP_MENU // to build the NTPMenu.bin (will perform the NTP Sync)
//#define RTC_PROFILE CHRONOMANIAC // to build the BLEMenu.bin with RTC support and NTP Sync
//#define RTC_PROFILE ROGUE // to build the BLEMenu.bin with RTC support, but *without* NTP sync
//#define RTC_PROFILE HOBO // to build the NTPMenu.bin *without* RTC/NTP support
byte SCAN_TIME = 10; // seconds 
#define MIN_SCAN_TIME 10 // seconds min
#define MAX_SCAN_TIME 50 // seconds max
#define BLEDEVCACHE_SIZE 16 // use some heap to cache BLECards, min = 5, max = 64, higher value = smaller uptime
#define VENDORCACHE_SIZE 32 // use some heap to cache vendor query responses, min = 5, max = 32
#define OUICACHE_SIZE 32 // use some heap to cache mac query responses, min = 16, max = 64
#define USE_NVS // comment this out if you have NVS problems (or just do an erase_flash)

#define NTP_MENU_NAME "NTPMenu"
#define BLE_MENU_NAME "BLEMenu"
#define RTC_SDA 26 // pin number
#define RTC_SCL 27 // pin number

// don't edit anything below this
#if RTC_PROFILE==HOBO // no NTP for Hobo mode
  #define BUILD_TYPE BLE_MENU_NAME
#elif RTC_PROFILE==ROGUE // no NTP for Rogue mode
  #define BUILD_TYPE BLE_MENU_NAME
#elif RTC_PROFILE==CHRONOMANIAC // no NTP for Chronomaniac
  #define BUILD_TYPE BLE_MENU_NAME
#elif RTC_PROFILE==NTP_MENU // enable NTP support, disable everything else
  // building 'NTPMenu.bin'
  #define BUILD_NTPMENU_BIN
  #define BUILD_TYPE NTP_MENU_NAME
  //#define WIFI_SSID "my-router-ssid"
  //#define WIFI_PASSWD "my-router-passwd"
#else
  #error "No valid RTC_PROFILE has been selected, please refer to the comments in Settings.h"
#endif

#define MENU_FILENAME "/" BUILD_TYPE ".bin"
#define NTP_MENU_FILENAME "/" NTP_MENU_NAME ".bin"
#define BLE_MENU_FILENAME "/" BLE_MENU_NAME ".bin"

#define BUILD_NEEDLE "ESP32 BLE Scanner Compiled On " // this 'signature' string must be unique in the whole source tree
#define BUILD_SIGNATURE __DATE__ " - " __TIME__ " - " BUILD_TYPE 
#define WELCOME_MESSAGE BUILD_NEEDLE BUILD_SIGNATURE
const char* needle = BUILD_NEEDLE;
const char* welcomeMessage = WELCOME_MESSAGE;
const char* buildSignature = BUILD_SIGNATURE;

#include <Adafruit_GFX.h>    // Core graphics library
#include "WROVER_KIT_LCD.h" // Must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
WROVER_KIT_LCD tft;

#if RTC_PROFILE > HOBO
  // RTC Module: On Wrover Kit you can use the following pins (from the camera connector)
  // SCL = GPIO27 (SIO_C / SCCB Clock 4)
  // SDA = GPIO26 (SIO_D / SCCB Data)
  #include <RTClib.h>
  #include <Wire.h>
  RTC_DS1307 RTC; // or your own RTC module
#endif

#ifndef BUILD_NTPMENU_BIN 
  // don't load BLE stack and SQLite3 when compiling the NTP Utility
  #include <BLEDevice.h>
  #include <BLEUtils.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
  // used to disable brownout detector
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
  #include <stdio.h>
  #include <stdlib.h>
  #include <sqlite3.h> // https://github.com/siara-cc/esp32_arduino_sqlite3_lib
#endif

#include <Preferences.h>
Preferences preferences;
#define MAX_ITEMS_IN_PREFS BLEDEVCACHE_SIZE // hopefully this will not overflow the 100k of NVRAM so don't assign a too high value or it will fail
#if MAX_ITEMS_IN_PREFS > BLEDEVCACHE_SIZE
  #error "MAX_ITEMS_IN_PREFS must be inferior or equal to BLEDEVCACHE_SIZE in Settings.h"
#endif


/*
 * Data sources:
 * - HEAP Cache : used as often as possible
 * - NVS Cache : used to freeze unsaved data, when heap is too low or DB oom occurs and reboot is required
 * - SQLite3 DB OUI : readonly, mainly a getter for vendor names by mac address 
 * - SQLite3 DB Vendor : readonly, mainly a getter for vendor names by manufacturer data
 * - SQLite3 DB blemacs : read/write, getter and setter for non anonymous BLE Advertised Devices
 * 
 */


#include <FS.h>
#include <SD_MMC.h>
// used to get the resetReason
#include <rom/rtc.h>

// because ESP.getFreeHeap() is inconsistent across SDK versions
// use the primitive... eats 25Kb memory
#define freeheap heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
#define resetReason (int)rtc_get_reset_reason(0)

// statistical values
int devicesCount = 0; // devices count per scan
int sessDevicesCount = 0; // total devices count per session
int newDevicesCount = 0; // total devices count per session
static int results = 0; // total results during last query
unsigned int entries = 0; // total entries in database
byte prune_trigger = 0; // incremented on every insertion, reset on prune()
byte prune_threshold = 10; // prune every x inertions
bool print_results = false;
bool print_tabular = true;

// load stack
#include "Assets.h" // bitmaps
#include "BLECache.h" // data struct
#include "ScrollPanel.h" // scrolly methods
#if RTC_PROFILE == CHRONOMANIAC ||  RTC_PROFILE == NTP_MENU
  #include "SDUpdater.h" // multi roms system
#endif
#include "TimeUtils.h" // RTC / NTP support
#include "UI.h"
#include "DB.h"
#include "BLE.h"
