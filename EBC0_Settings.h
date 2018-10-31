// software library stack
#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "WROVER_KIT_LCD.h" // Must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
// RTC Module: On Wrover Kit you can use the following pins (from the camera connector)
// SCL = GPIO27 (SIO_C / SCCB Clock 4)
// SDA = GPIO26 (SIO_D / SCCB Data)
#include <RTClib.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> // https://github.com/siara-cc/esp32_arduino_sqlite3_lib
#include <SPI.h>
#include <FS.h>
#include <SD_MMC.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <rom/rtc.h>

// constructors
WROVER_KIT_LCD tft;
RTC_DS1307 RTC; // or your own RTC module
sqlite3 *BLECollectorDB; // read/write
sqlite3 *BLEVendorsDB; // readonly
sqlite3 *OUIVendorsDB; // readonly

// because ESP.getFreeHeap() is inconsistent across SDK versions
// use the primitive... eats 25Kb memory
#define freeheap heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
#define SCAN_TIME  30 // seconds
// some UI settings
#define MAX_FIELD_LEN 32 // max chars returned by field
#define MAX_ROW_LEN 30 // max chars per line on display
// blinky (topleft) BLE icon
#define ICON_X 40
#define ICON_Y 4
#define ICON_R 4
// blescan progress bar
#define PROGRESSBAR_Y 30
// top and bottom non-scrolly zones
#define HEADER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
#define FOOTER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
#define HEADER_HEIGHT 36 // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
#define FOOTER_HEIGHT 36 // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
#define SCROLL_HEIGHT ( tft.height() - ( HEADER_HEIGHT + FOOTER_HEIGHT ))
// middle scrolly zone
#define BLECARD_BGCOLOR tft.color565(0x22, 0x22, 0x44)
#define HEAPMAP_BUFFLEN 81 // graph width (+ 1 for hscroll)
// now throw all sorts of global variables :-)

// scroll control variables
uint16_t scrollTopFixedArea = 0;
uint16_t scrollBottomFixedArea = 0;
uint16_t yStart = scrollTopFixedArea;
uint16_t tft_height = tft.height();//ILI9341_HEIGHT (=320)
uint16_t tft_width  = tft.width();//ILI9341_WIDTH (=240)
uint16_t yArea = tft_height - scrollTopFixedArea - scrollBottomFixedArea;
int16_t  x1_tmp, y1_tmp;
uint16_t w_tmp, h_tmp;
int scrollPosY = -1;
int scrollPosX = -1;
const String SPACETABS = "      ";
const String SPACE = " ";
// heap management by graph
uint32_t GRAPH_LINE_WIDTH = HEAPMAP_BUFFLEN-1; 
uint32_t GRAPH_LINE_HEIGHT = 30;
uint16_t GRAPH_X = tft_width - GRAPH_LINE_WIDTH - 2;
uint16_t GRAPH_Y = 287;
uint32_t min_free_heap = 100000; // sql out of memory errors occur under 120000
uint32_t initial_free_heap;
uint32_t heap_tolerance = 20000; // how much memory under minimum the sketch can go
uint32_t heapmap[HEAPMAP_BUFFLEN] = {0};
byte heapindex = 0;


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
char *colNeedle = 0; // search criteria
String colValue = ""; // search result
const char *welcomeMSG = "ESP32    BLE Collector";

bool RTC_is_running = false;
static char timeString[13] = "00:00"; // %02d:%02d
static char UpTimeString[13] = "00:00"; // %02d:%02d
static DateTime nowDateTime;


void updateTimeString() {
  nowDateTime = RTC.now();

  unsigned long seconds_since_boot = millis() / 1000;
  uint16_t mm = seconds_since_boot / 60;
  uint16_t hh = mm / 60;
  sprintf(UpTimeString, " %02d:%02d ", hh, mm);
  // calculate a date which is 7 days and 30 seconds into the future
  // DateTime future (now + TimeSpan(7,12,30,6));
  sprintf(timeString, " %02d:%02d ", nowDateTime.hour(), nowDateTime.minute());
  Serial.println("Time:" + String(timeString) + " Uptime:" + String(UpTimeString));
}

// load stack
#include "assets.h"
#include "EBC1_BLEstructs.h"
#include "EBC2_OutputService.h"
#include "EBC3_UIUtils.h"
#include "EBC4_DBUtils.h"
#include "EBC5_BLEScanUtils.h"
