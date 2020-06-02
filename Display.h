#include <ESP32-Chimera-Core.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater

#ifndef _CHIMERA_CORE_
  #warning "This app needs ESP32 Chimera Core but M5Stack Core was selected, check your library path !!"
  #include <SD.h>
  #define M5STACK_SD SD
#else
  #define tft M5.Lcd // syntax sugar
#endif

#include "HID_XPad.h" // external HID


#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE ) || defined( ARDUINO_ODROID_ESP32 ) || defined ( ARDUINO_ESP32_DEV ) || defined( ARDUINO_DDUINO32_XS )
  // yay! platform is supported
#else
  #error "NO SUPPORTED BOARD DETECTED !!"
  #error "Please select the right board from the Arduino boards menu"
  #error "Supported boards are: ARDUINO_M5Stack_Core_ESP32, ARDUINO_M5STACK_FIRE, ARDUINO_ODROID_ESP32 or ARDUINO_ESP32_DEV"
#endif

#define BLE_FS M5STACK_SD // inherited from ESP32-Chimera-Core

#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define scrollpanel_height() tft.height()
#define scrollpanel_width() tft.width()
#define tft_initOrientation() tft.setRotation(1)
#define tft_drawBitmap tft.drawBitmap
#define SD_begin M5StackSDBegin // BLE_FS.begin
#define hasHID() (bool)true
#define hasXPaxShield() (bool) false
#define snapNeedsScrollReset() (bool)false // some TFT models need a scroll reset before screen capture
#define BLE_FS_TYPE "sd" // sd = fs::SD, sdcard = fs::SD_MMC
#define SKIP_INTRO // don't play intro (tft spi access messes up SD/DB init)
static const int AMIGABALL_YPOS = 50;
#define BASE_BRIGHTNESS 32 // multiple of 8 otherwise can't turn off ^^
#define SCROLL_OFFSET 0 // tardis definition (some ST7789 needs 320x vertical scrolling adressing but can be limited to 240x240)

// uncomment this block to use SPIFFS instead of SD
// WARNING: can only work with big SPIFFS partition (minumum 2MB, ESP32-WROVER chips only)
//#undef BLE_FS
//#undef BLE_FS_TYPE
//#define BLE_FS SPIFFS // inherited from ESP32-Chimera-Core
//#define BLE_FS_TYPE "spiffs" // sd = fs::SD, sdcard = fs::SD_MMC

// Experimental, requires an ESP32-Wrover/Odroid-Go/M5Fire, a huge partition scheme and no OTA
//#define WITH_WIFI


// display profiles switcher
#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE ) || defined( ARDUINO_ODROID_ESP32 )

  // custom M5Stack/Odroid-Go go TFT/SD/RTC/GPS settings here (see ARDUINO_ESP32_DEV profile for available settings)

#elif defined( ARDUINO_DDUINO32_XS )

  #undef hasHID
  #undef SD_begin
  #undef scrollpanel_height
  #undef scrollpanel_width
  #undef tft_initOrientation
  #undef BLE_FS_TYPE
  #undef SCROLL_OFFSET

  #define hasHID() (bool)false // disable buttons
  #define SD_begin /*(bool)true*/BLE_FS.begin // SD_MMC is auto started
  #define tft_initOrientation() tft.setRotation(0) // default orientation for hardware scroll
  #define scrollpanel_height() tft.width() // invert these if scroll fails
  #define scrollpanel_width() tft.height() // invert these if scroll fails
  #define BLE_FS_TYPE "sdcard" // sd = fs::SD, sdcard = fs::SD_MMC
  #warning D-Duino32-XS detected !!
  // ST7789 uses 320x hardware vscroll def, but this model is 240x240 !!
  // https://www.rhydolabz.com/documents/33/ST7789.pdf
  // restriction: The condition is TFA+VSA+BFA = 320, otherwise Scrolling mode is undefined.
  #define SCROLL_OFFSET 320-240

#elif defined ( ARDUINO_ESP32_DEV )

  // since C macros are lazy, overwrite the settings.h values
  #undef HAS_EXTERNAL_RTC
  #undef HAS_GPS
  #undef hasHID
  #undef hasXPaxShield
  #undef snapNeedsScrollReset
  #undef SD_begin
  #undef scrollpanel_height
  #undef scrollpanel_width
  #undef tft_initOrientation
  #undef RTC_SDA
  #undef RTC_SCL
  #undef GPS_RX
  #undef GPS_TX
  #undef BLE_FS_TYPE
  #undef BASE_BRIGHTNESS
  #define BASE_BRIGHTNESS 128

  #define HAS_EXTERNAL_RTC true // will use RTC_SDA and RTC_SCL from settings.h
  #define HAS_GPS true // will use GPS_RX and GPS_TX from settings.h
  #define hasHID() (bool)false // disable buttons
  #define hasXPaxShield() (bool) true
  #define snapNeedsScrollReset() (bool)true
  #define SD_begin /*(bool)true*/BLE_FS.begin // SD_MMC is auto started
  #define tft_initOrientation() tft.setRotation(0) // default orientation for hardware scroll
  #define scrollpanel_height() tft.height() // invert these if scroll fails
  #define scrollpanel_width() tft.width() // invert these if scroll fails
  #define RTC_SDA 26 // pin number
  #define RTC_SCL 27 // pin number
  #define GPS_RX 39 // io pin number
  #define GPS_TX 35 // io pin number
  #define BLE_FS_TYPE "sdcard" // "sd" = fs::SD, "sdcard" = fs::SD_MMC

  #warning WROVER KIT DETECTED !!

#else

  #error "NO SUPPORTED BOARD DETECTED !!"
  #error "Either define one of the supported boards, add it to the profile selection, or uncomment this error to experiment"

#endif


static TFT_eSprite gradientSprite( &tft );  // gradient background
static TFT_eSprite heapGraphSprite( &tft ); // activity graph
static TFT_eSprite hallOfMacSprite( &tft ); // mac address badge holder


// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}


void tft_begin() {
  M5.begin( true, true, false, false, false ); // don't start Serial
  //tft.init();
  //M5.ScreenShot.init( &tft, BLE_FS );
  //M5.ScreenShot.begin();

  #if HAS_EXTERNAL_RTC
    //Wire.begin(RTC_SDA, RTC_SCL);
    //M5.I2C.scan();
  #endif
  delay( 100 );

  //tft.init();
  //tft.setRotation(1);

  #ifdef __M5STACKUPDATER_H
    if( hasHID() ) {
      // build has buttons => enable SD Updater at boot
      if(digitalRead(BUTTON_A_PIN) == 0) {
        Serial.println("Will Load menu binary");
        updateFromFS();
        ESP.restart();
      }
    } else if( hasXPaxShield() ) {
      XPadShield.init();
      XPadShield.update();
      if( XPadShield.BtnA->wasPressed() ) {
        Serial.println("Will Load menu binary");
        updateFromFS();
        ESP.restart();
      }
    }
  #endif
}


void tft_setBrightness( uint8_t brightness ) {
  tft.setBrightness( brightness );
}

// emulating Adafruit's tft.getTextBounds()
void tft_getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( string );
  *h = tft.fontHeight();
}
/*
void tft_getTextBounds(const __FlashStringHelper *s, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( s );
  *h = tft.fontHeight();
}*/
void tft_getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( str.c_str() );
  *h = tft.fontHeight();
}

void tft_fillCircle( uint16_t x, uint16_t y, uint16_t r, uint16_t color) {
  tft.fillCircle(x, y, r, color);
};
void tft_drawCircle( uint16_t x, uint16_t y, uint16_t r, uint16_t color) {
  tft.drawCircle(x, y, r, color);
};
void tft_fillRect( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  tft.fillRect(x, y, w, h, color);
};
void tft_fillTriangle( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}


// software scroll
/*
void tft_setupScrollArea(uint16_t tfa, uint16_t vsa, uint16_t bfa) {
  log_w("Init Software Scroll area with tft.setScrollRect( 0, %d, %d, %d )", tfa, tft.width(), vsa);
  tft.setScrollRect( 0, tfa, tft.width(), vsa );
  //tft.getScrollRect( &s_x_tmp, &s_y_tmp, &s_w_tmp, &s_h_tmp );
}
*/

// software scroll
static int32_t s_x_tmp, s_y_tmp, s_w_tmp, s_h_tmp;
static uint16_t circularScrollBufferHSize = 240;
static uint16_t circularScrollBufferVSize = 16;
static uint16_t circularScrollBufferSize = circularScrollBufferHSize*circularScrollBufferVSize;
void tft_scrollTo(int32_t vsp) {
  if( vsp == 0 ) return;
  //tft.getScrollRect( &s_x_tmp, &s_y_tmp, &s_w_tmp, &s_h_tmp );
  int32_t direction   = vsp>0 ? 1 : -1;
  int32_t blockHeight = abs(vsp);
  int32_t blockSize   = s_w_tmp*blockHeight;
  int32_t bottomY     = (s_y_tmp+s_h_tmp)-blockHeight;

  if( blockSize <= circularScrollBufferSize ) {
    // scroll area fits into buffer
    RGBColor * scrollArea = new RGBColor[s_w_tmp*blockHeight];
    int32_t ySrc        = 0;
    int32_t yDest       = 0;
    if( vsp < 0 ) {
      ySrc  = s_y_tmp;
      yDest = bottomY;
    } else {
      ySrc  = bottomY;
      yDest = s_y_tmp;
    }
    tft.readRectRGB( s_x_tmp, ySrc,  s_w_tmp, blockHeight, scrollArea );
    tft.scroll(0, vsp);
    tft.pushImage(   s_x_tmp, yDest, s_w_tmp, blockHeight, scrollArea );
    log_v("[block] tft.scroll(0, %d); scrollRect( %d, %d, %d, %d ) ", vsp, s_x_tmp, s_y_tmp, s_w_tmp, s_h_tmp );
    free( scrollArea );
  } else {
    // scroll area doesn't fit into buffer, need to split
    log_v("[split] tft.scroll(0, %d); scrollRect( %d, %d, %d, %d ) ", vsp, s_x_tmp, s_y_tmp, s_w_tmp, s_h_tmp );
    // how many scan lines can the buffer fit ?
    int scanLines = floor( circularScrollBufferSize / s_w_tmp );
    // how many iterations these scanLines will be required to cover the current scroll zone
    int iterations = floor( blockHeight / scanLines );
    // are there any leftover lines
    int leftover = blockHeight%scanLines;
    // go recursive
    while( iterations-- > 0 ) {
      tft_scrollTo( scanLines * direction );
    }
    // do leftover lines if any
    tft_scrollTo( leftover * direction );
  }
}

// hardware scroll
void tft_setupHScrollArea(uint16_t tfa, uint16_t vsa, uint16_t bfa) {
  bfa += SCROLL_OFFSET; // compensate for stubborn firmware
  tft.writecommand(0x33/*ILI9341_VSCRDEF*/); // Vertical scroll definition
  tft.writedata(tfa >> 8);           // Top Fixed Area line count
  tft.writedata(tfa);
  tft.writedata(vsa >> 8);  // Vertical Scrolling Area line count
  tft.writedata(vsa);
  tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
  tft.writedata(bfa);
  log_w("Init Hardware Scroll area with tfa/vsa/bfa %d/%d/%d on w/h %d/%d", tfa, vsa, bfa, scrollpanel_width(), scrollpanel_height());
}
// hardware scroll
void tft_hScrollTo(uint16_t vsp) {
  tft.writecommand(0x37/*ILI9341_VSCRSADD*/); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
}


void tft_fillGradientHRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor colorstart, RGBColor colorend ) {
  log_v("tft_fillGradientHRect( %d, %d, %d, %d )\n", x, y, width, height );
  gradientSprite.setPsram( false ); // don't bother using psram for that
  //gradientSprite.setSwapBytes( false );
  gradientSprite.setColorDepth( 16 );
  gradientSprite.createSprite( width, 1);
  tft.startWrite();
  gradientSprite.drawGradientHLine( 0, 0, width, colorstart, colorend );
  for( uint16_t h = 0; h < height; h++ ) {
    gradientSprite.pushSprite( x, y+h );
  }
  tft.endWrite();
  gradientSprite.deleteSprite();
}

void tft_fillGradientVRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor colorstart, RGBColor colorend ) {
  gradientSprite.setPsram( false ); // don't bother using psram for that
  //gradientSprite.setSwapBytes( false );
  gradientSprite.setColorDepth( 16 );
  gradientSprite.createSprite( 1, height);
  tft.startWrite();
  gradientSprite.drawGradientVLine( 0, 0, height, colorstart, colorend );
  for( uint16_t w = 0; w < width; w++ ) {
    gradientSprite.pushSprite( x+w, y );
  }
  tft.endWrite();
  gradientSprite.deleteSprite();
}

void tft_drawGradientHLine( uint32_t x, uint32_t y, uint32_t w, RGBColor colorstart, RGBColor colorend ) {
  tft_fillGradientHRect( x, y, w, 1, colorstart, colorend );
}

void tft_drawGradientVLine( uint32_t x, uint32_t y, uint32_t h, RGBColor colorstart, RGBColor colorend ) {
  tft_fillGradientVRect( x, y, 1, h, colorstart, colorend );
}

enum TextDirections {
  ALIGN_FREE   = 0,
  ALIGN_LEFT   = 1,
  ALIGN_RIGHT  = 2,
  ALIGN_CENTER = 3,
};


enum BLECardThemes {
  IN_CACHE_ANON = 0,
  IN_CACHE_NOT_ANON = 1,
  NOT_IN_CACHE_ANON = 2,
  NOT_IN_CACHE_NOT_ANON = 3
};

typedef enum {
  TFT_SQUARE = 0,
  TFT_PORTRAIT = 1,
  TFT_LANDSCAPE = 2
} DisplayMode;

static DisplayMode displayMode;

static uint8_t percentBoxSize;
static uint8_t headerLineHeight;
static uint8_t leftMargin;
static uint8_t iconR;// = 4; // BLE icon radius
static uint8_t macAddrColorsScaleX;
static uint8_t macAddrColorsScaleY;

static int16_t graphX;
static int16_t graphY;
static int16_t percentBoxX;
static int16_t percentBoxY;
static int16_t headerStatsX;
static int16_t footerBottomPosY;
static int16_t headerStatsIconsX;
static int16_t headerStatsIconsY;
static int16_t progressBarY;
static int16_t hhmmPosX;
static int16_t hhmmPosY;
static int16_t uptimePosX;
static int16_t uptimePosY;
static int16_t copyleftPosX;
static int16_t copyleftPosY;
static int16_t cdevcPosX;
static int16_t cdevcPosY;
static int16_t sesscPosX;
static int16_t sesscPosY;
static int16_t ndevcPosX;
static int16_t ndevcPosY;
// icon positions for RTC/DB/BLE
__attribute__((unused)) static int16_t iconAppX;
__attribute__((unused)) static int16_t iconAppY;
static int16_t iconRtcX;
static int16_t iconRtcY;
static int16_t iconBleX;
static int16_t iconBleY;
static int16_t iconDbX;
static int16_t iconDbY;
static int16_t macAddrColorsPosX;
static int16_t heapStrX;
static int16_t heapStrY;
static int16_t entriesStrX;
static int16_t entriesStrY;
static int16_t BLECollectorIconBarM;
static int16_t BLECollectorIconBarX;
static int16_t BLECollectorIconBarY;
static int16_t hallOfMacPosX;
static int16_t hallOfMacPosY;

static uint16_t hallOfMacHmargin;
static uint16_t hallOfMacVmargin;
static uint16_t hallOfMacSize;
static uint16_t hallofMacCols;
static uint16_t hallofMacRows;
static uint16_t hallOfMacItemWidth;
static uint16_t hallOfMacItemHeight;
static uint16_t macAddrColorsSizeX;
static uint16_t macAddrColorsSizeY;
static uint16_t headerHeight;
static uint16_t footerHeight;
static uint16_t scrollHeight;
static uint16_t graphLineWidth;
static uint16_t graphLineHeight;

static uint16_t prevx;
static uint16_t prevy;

static int32_t macAddrColorsSize;

static bool showScanStats;
static bool showHeap;
static bool showEntries;
static bool showCdevc;
static bool showSessc;
static bool showNdevc;
static bool showUptime;

static TextDirections entriesAlign;
static TextDirections heapAlign;
static TextDirections cdevcAlign;
static TextDirections sesscAlign;
static TextDirections ndevcAlign;
static TextDirections uptimeAlign;

static char seenDevicesCountSpacer[5];// = ""; // Seen
static char scansCountSpacer[5];// = ""; // Scans

// UI palette
static const uint16_t BLE_WHITE       = 0xFFFF;
static const uint16_t BLE_BLACK       = 0x0000;
static const uint16_t BLE_GREEN       = 0x07E0;
static const uint16_t BLE_YELLOW      = tft_color565(0xff, 0xff, 0x00); // 0xFFE0;
static const uint16_t BLE_GREENYELLOW = 0xAFE5;
static const uint16_t BLE_CYAN        = 0x07FF;
static const uint16_t BLE_ORANGE      = 0xFD20;
static const uint16_t BLE_DARKGREY    = 0x7BEF;
static const uint16_t BLE_LIGHTGREY   = 0xC618;
static const uint16_t BLE_RED         = 0xF800;
static const uint16_t BLE_DARKGREEN   = 0x03E0;
static const uint16_t BLE_DARKBLUE    = tft_color565(0x22, 0x22, 0x44);
static const uint16_t BLE_PURPLE      = 0x780F;
static const uint16_t BLE_PINK        = 0xF81F;
static const uint16_t BLE_TRANSPARENT = TFT_TRANSPARENT;

// top and bottom non-scrolly zones
static const uint16_t HEADER_BGCOLOR      = tft_color565(0x22, 0x22, 0x22);
static const uint16_t FOOTER_BGCOLOR      = tft_color565(0x22, 0x22, 0x22);
// BLECard info styling
static const uint16_t IN_CACHE_COLOR      = tft_color565(0x87, 0xff, 0x87);
static const uint16_t NOT_IN_CACHE_COLOR  = tft_color565(0xff, 0xa0, 0x5f);
static const uint16_t ANONYMOUS_COLOR     = tft_color565(0xaa, 0xcc, 0xcc);
static const uint16_t NOT_ANONYMOUS_COLOR = tft_color565(0xee, 0xff, 0xee);
// one carefully chosen blue
static const uint16_t BLUETOOTH_COLOR     = tft_color565(0x14, 0x54, 0xf0);
static const uint16_t BLE_DARKORANGE      = tft_color565(0x80, 0x40, 0x00);
// middle scrolly zone
static const uint16_t BLECARD_BGCOLOR     = tft_color565(0x22, 0x22, 0x44);
// placehorder for **variable** background color
static uint16_t BGCOLOR                   = tft_color565(0x22, 0x22, 0x44);

// heap map settings

uint32_t lastfreeheap;
uint32_t toleranceheap;
uint16_t baseCoordY;
uint16_t dcpmFirstY;
uint16_t dcpmLastX = 0;
uint16_t dcpmLastY = 0;
uint16_t dcpmppFirstY;
uint16_t dcpmppLastX = 0;
uint16_t dcpmppLastY = 0;
#define COPYLEFT_SIGN "(c+)"
#define AUTHOR "tobozo"

// heap management (used by graph)
static uint32_t heapMapBuffLen = 61; // graph width (+ 1 for hscroll)
static uint32_t min_free_heap = 90000; // sql out of memory errors eventually occur under 100000
static uint32_t initial_free_heap = freeheap;
static uint32_t heap_tolerance = 20000; // how much memory under min_free_heap the sketch can go and recover without restarting itself
static uint32_t *heapmap = NULL;//[HEAPMAP_BUFFLEN] = {0}; // stores the history of heapmap values
static uint16_t heapindex = 0; // index in the circular buffer

size_t devicesStatCount = 0;    // how many devices found since last measure
unsigned long lastDeviceStatCount = 0; // when the last devices count reset was made

unsigned long devGraphFirstStatTime = millis();
unsigned long devGraphStartedSince = 0;
const unsigned long devGraphPeriodShort = 1000; // refresh every 1 second
const unsigned long devGraphPeriodLong  = 1000 * 5; // refresh every 5 seconds
uint16_t devCountPerMinute[60] = {0};
uint16_t *devCountPerMinutePerPeriod = NULL;//[HEAPMAP_BUFFLEN] = {0};
uint8_t devCountPerMinuteIndex = 0;
uint8_t devCountPerMinutePerPeriodIndex = 0;
int8_t minutesTimeZone = 0;

uint16_t maxcdpm = 0;
uint16_t mincdpm = 0xffff;

uint16_t maxcdpmpp = 0;
uint16_t mincdpmpp = 0xffff;

static bool devCountWasUpdated = false;
static bool blinkit = false; // task blinker state
static bool blinktoggler = true;
static bool appIconRendered = false;
static bool foundTimeServer = false;
static bool foundFileServer = false;
static bool RTCisRunning = false;
static bool ForceBleTime = false;
static bool HasBTTime = false;
static bool RamCacheReady = false;

static unsigned long blinknow = millis(); // task blinker start time
static unsigned long scanTime = SCAN_DURATION * 1000; // task blinker duration
static unsigned long blinkthen = blinknow + scanTime; // task blinker end time
static unsigned long lastblink = millis(); // task blinker last blink
static unsigned long lastprogress = millis(); // task blinker progress

const char* seenDevicesCountTpl = "Seen:%s%4s";
const char* scansCountTpl = "Scans:%s%4s";
const char* heapTpl = "Heap: %6d";
const char* entriesTpl = "Entries:%4s";
const char* addressTpl = "  %s";
const char* dbmTpl = "%ddBm    ";
const char* ouiTpl = "      %s";
const char* appearanceTpl = "  Appearance: %d";
const char* manufTpl = "      %s";
const char* nameTpl = "      %s";
const char* screenshotFilenameTpl = "/screenshot-%04d-%02d-%02d_%02dh%02dm%02ds.565";
const char* hitsTimeStampTpl = "      %04d/%02d/%02d %02d:%02d:%02d %s";

static char textWidgetStr[17] = { 0 };
static char unitOutput[16] = {'\0'};
static char addressStr[24] = {'\0'};
static char dbmStr[16] = {'\0'};
static char hitsTimeStampStr[48] = {'\0'};
static char hitsStr[16] = {'\0'};
static char nameStr[38] = {'\0'};
static char ouiStr[38] = {'\0'};
static char appearanceStr[48] = {'\0'};
static char manufStr[38] = {'\0'};

// some date/time formats used in this app
const char* hhmmStringTpl = "%02d:%02d";
static char hhmmString[13] = "--:--";
const char* hhmmssStringTpl = "%02d:%02d:%02d";
static char hhmmssString[13] = "--:--:--";
const char* UpTimeStringTpl = "%02d:%02d";
const char* UpTimeStringTplDays = "%2d %s";
static char UpTimeString[32] = "--:--";
static char UpTimeStringTplTpl[16] = "";
const char* YYYYMMDD_HHMMSS_Tpl = "%04d-%02d-%02d %02d:%02d:%02d";
static char YYYYMMDD_HHMMSS_Str[32] = "YYYY-MM-DD HH:MM:SS";
static bool DayChangeTrigger = false;
static bool HourChangeTrigger = false;
int current_day = -1;
int current_hour = -1;

extern bool scanTaskRunning;
