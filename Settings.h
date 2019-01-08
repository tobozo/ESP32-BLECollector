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
    different modes, see "#define SKETCH_MODE"
 
*/
// don't edit those
//#define HOBO 1 // No TinyRTC module in your build, can use BLETimeServer otherwise only uptime will be displayed
#define HOBO         1 // wtf
#define ROGUE        2 // TinyRTC module adjusted after flashing, no WiFi NTP Sync, can use BLETimeServer
#define CHRONOMANIAC 3 // TinyRTC module adjusts itself via BLE or NTP by sd-loading a separate binary
#define NTP_MENU     4 // use this to produce the NTPMenu.bin, only if you have a RTC module !!
#define TIME_UPDATE_NONE 0 // update from nowhere, only using uptime as timestamps
#define TIME_UPDATE_BLE  1 // update from BLE Time Server (see the /tools directory of this project)
#define TIME_UPDATE_NTP  2 // update from NTP Time Server (requires WiFi and a separate build)
#define TIME_UPDATE_GPS  3 // update from GPS external module (to be implemented)
#define SKETCH_MODE_BUILD_DEFAULT     1 // build the BLE Collector
#define SKETCH_MODE_BUILD_NTP_UPDATER 2 // build the NTP Updater for external RTC

// edit these values to fit your mode
#define HAS_EXTERNAL_RTC true
#define TIME_UPDATE_SOURCE     TIME_UPDATE_BLE
#define SKETCH_MODE     SKETCH_MODE_BUILD_DEFAULT
//#define SKETCH_MODE     SKETCH_MODE_BUILD_NTP_UPDATER

byte SCAN_DURATION = 20; // seconds, will be adjusted upon scan results
#define MIN_SCAN_DURATION 10 // seconds min
#define MAX_SCAN_DURATION 120 // seconds max
#define VENDORCACHE_SIZE 16 // use some heap to cache vendor query responses, min = 5, max = 256
#define OUICACHE_SIZE 8 // use some heap to cache mac query responses, min = 16, max = 4096
#define MAX_FIELD_LEN 32 // max chars returned by field
#define MAC_LEN 17 // chars used by a mac address
#define SHORT_MAC_LEN 7 // chars used by the oui part of a mac address

// don't edit anything below this

#define NTP_MENU_NAME "NTPMenu"
#define BLE_MENU_NAME "BLEMenu"

#if SKETCH_MODE==SKETCH_MODE_BUILD_DEFAULT
  #define BUILD_TYPE BLE_MENU_NAME
  #if HAS_EXTERNAL_RTC
   #if TIME_UPDATE_SOURCE==TIME_UPDATE_NTP
     #define RTC_PROFILE "CHRONOMANIAC"
     #define NEEDS_SDUPDATER
   #else
     #define RTC_PROFILE "ROGUE"
   #endif
  #else
    #define RTC_PROFILE "HOBO"
  #endif
#else
  #define BUILD_TYPE NTP_MENU_NAME
  #define RTC_PROFILE "NTP_MENU"
  #define NEEDS_SDUPDATER true
  //#define WIFI_SSID "my-router-ssid"
  //#define WIFI_PASSWD "my-router-passwd
#endif

#define MAX_BLECARDS_WITH_TIMESTAMPS_ON_SCREEN 4
#define MAX_BLECARDS_WITHOUT_TIMESTAMPS_ON_SCREEN 5
#define BLEDEVCACHE_PSRAM_SIZE 1024 // use PSram to cache BLECards
#define BLEDEVCACHE_HEAP_SIZE 32 // use some heap to cache BLECards. min = 5, max = 64, higher value = smaller uptime
#define MAX_DEVICES_PER_SCAN MAX_BLECARDS_WITH_TIMESTAMPS_ON_SCREEN // also max displayed devices on the screen, affects initial scan duration

#define MENU_FILENAME "/" BUILD_TYPE ".bin"
#define NTP_MENU_FILENAME "/" NTP_MENU_NAME ".bin"
#define BLE_MENU_FILENAME "/" BLE_MENU_NAME ".bin"

#define BUILD_NEEDLE "ESP32 BLE Scanner Compiled On " // this 'signature' string must be unique in the whole source tree
#define BUILD_SIGNATURE __DATE__ " - " __TIME__ " - " BUILD_TYPE 
#define WELCOME_MESSAGE BUILD_NEEDLE BUILD_SIGNATURE
const char* needle = BUILD_NEEDLE;
const char* welcomeMessage = WELCOME_MESSAGE;
const char* BUILDSIGNATURE = BUILD_SIGNATURE;
uint32_t sizeofneedle = strlen(needle);
uint32_t sizeoftrail = strlen(welcomeMessage) - sizeofneedle;

static xSemaphoreHandle mux = NULL; // this is needed to prevent rendering collisions 
                                    // between scrollpanel and heap graph
int8_t timeZone = 1;
int8_t minutesTimeZone = 0;
const char* NTP_SERVER = "europe.pool.ntp.org";
static bool RTCisRunning = false;
static bool ForceBleTime = false;
static bool HasBTTime = false;
// some date/time formats used in this app
const char* hhmmStringTpl = "  %02d:%02d  ";
static char hhmmString[13] = "  --:--  ";
const char* hhmmssStringTpl = "%02d:%02d:%02d";
static char hhmmssString[13] = "--:--:--"; 
const char* UpTimeStringTpl = "  %02d:%02d  ";
static char UpTimeString[13] = "  --:--  ";
const char* YYYYMMDD_HHMMSS_Tpl = "%04d-%02d-%02d %02d:%02d:%02d";
static char YYYYMMDD_HHMMSS_Str[32] = "YYYY-MM-DD HH:MM:SS";
static bool DayChangeTrigger = false;
static bool HourChangeTrigger = false;
static bool DBneedsReplication = false;

int current_day = -1;
int current_hour = -1;

// used to get the resetReason
#include <rom/rtc.h>
#include <Preferences.h>
Preferences preferences;
#include "Display.h"
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include "DateTime.h"

#if HAS_EXTERNAL_RTC
  #include <Wire.h>
  // RTC Module: On Wrover Kit you can use the following pins (from the camera connector)
  // SCL = GPIO27 (SIO_C / SCCB Clock 4)
  // SDA = GPIO26 (SIO_D / SCCB Data)
  #include "RTC.h"
  static BLE_RTC_DS1307 RTC;
  #define RTC_SDA 26 // pin number
  #define RTC_SCL 27 // pin number
#endif

#if SKETCH_MODE==SKETCH_MODE_BUILD_NTP_UPDATER
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <NtpClientLib.h> // https://github.com/gmag11/NtpClient
  #include "certificates.h"
#else
  // don't load BLE stack and SQLite3 when compiling the NTP Utility
  #include <BLEDevice.h>
  #include <BLEUtils.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
  // used to disable brownout detector
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
  #include "esp_task_wdt.h"

  #include <stdio.h>
  #include <stdlib.h>
  #include <sqlite3.h> // https://github.com/siara-cc/esp32_arduino_sqlite3_lib
#endif


/*
 * Data sources:
 * - HEAP Cache : used if no SPIRAM detected for BLEDEV, OUI and Vendors
 * - SPIRAM BLEDEV Cache L1 : all scanned BLE Devices are copied there for further analysis
 * - SPIRAM BLEDEV Cache L2 : all returning BLE Devices are copied there for hits counting
 * - SPIRAM OUI Lookup : OUI/Mac Database is copied there
 * - SPIRAM Vendor Lookup : Manufacturer names/id Database is copied there
 * - SQLite3 DB OUI : readonly, mainly a getter for vendor names by mac address 
 * - SQLite3 DB Vendor : readonly, mainly a getter for vendor names by manufacturer data
 * - SQLite3 DB blemacs : read/write, getter and setter for non anonymous BLE Advertised Devices
 * 
 */

// use the primitive because ESP.getFreeHeap() is inconsistent across SDK versions
#define freeheap heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
#define freepsheap ESP.getFreePsram()
#define resetReason (int)rtc_get_reset_reason(0)
#define takeMuxSemaphore() if( mux ) { xSemaphoreTake(mux, portMAX_DELAY); log_v("Took Semaphore"); }
#define giveMuxSemaphore() if( mux ) { xSemaphoreGive(mux); log_v("Gave Semaphore"); }

// statistical values
static int devicesCount = 0; // devices count per scan
static int sessDevicesCount = 0; // total devices count per session
static int newDevicesCount = 0; // total devices count per session
static int results = 0; // total results during last query
static unsigned int entries = 0; // total entries in database
static byte prune_trigger = 0; // incremented on every insertion, reset on prune()
static byte prune_threshold = 10; // prune every x inertions
//static bool print_results = false;
static bool print_tabular = true;

// load stack
#include "Assets.h" // bitmaps
#include "AmigaBall.h"
#include "SDUtils.h"
#include "BLECache.h" // data struct
#include "ScrollPanel.h" // scrolly methods
#ifdef NEEDS_SDUPDATER
  #include "SDUpdater.h" // multi roms system
#endif
#include "NTP.h"
#include "TimeUtils.h"
#include "UI.h"
#include "DB.h"
#include "BLE.h"
