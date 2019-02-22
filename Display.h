
#define WROVER_KIT
//#define M5STACK


#ifdef WROVER_KIT

#include <FS.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include "WROVER_KIT_LCD.h" // Latest version must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
WROVER_KIT_LCD tft;

#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
#define BLE_FS_TYPE "sdcard"
#define SD_begin() BLE_FS.begin()

#define tft_setupScrollArea tft.setupScrollArea
#define tft_scrollTo tft.scrollTo
#define tft_getTextBounds tft.getTextBounds
#define tft_readPixels tft.readPixels
#define tft_initOrientation() tft.setRotation(0)
#define scrollpanel_height() tft.height()
#define scrollpanel_width() tft.width()


#elif defined(M5STACK)


#include <M5Stack.h>
#include <M5StackUpdater.h>

M5Display tft;

#include <SD.h>
fs::SDFS &BLE_FS = SD;
#define BLE_FS_TYPE "sd"
#define SD_begin() BLE_FS.begin()

#define tft_readPixels tft.readRect
#define tft_initOrientation() tft.setRotation(1)
#define scrollpanel_height() tft.width()
#define scrollpanel_width() tft.height()

void tft_getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( string );
  *h = tft.fontHeight( tft.textfont );  
}
void tft_getTextBounds(const __FlashStringHelper *s, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( s );
  *h = tft.fontHeight( tft.textfont );  
}
void tft_getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  *w = tft.textWidth( str );
  *h = tft.fontHeight( tft.textfont );  
}


void tft_setupScrollArea(uint16_t tfa, uint16_t bfa) {
  tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  tft.writedata(tfa >> 8);           // Top Fixed Area line count
  tft.writedata(tfa);
  tft.writedata((scrollpanel_width()-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  tft.writedata(scrollpanel_width()-tfa-bfa);
  tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
  tft.writedata(bfa);
  log_e("Init Scroll area with tfa/bfa %d/%d on w/h %d/%d", tfa, bfa, scrollpanel_width(), scrollpanel_height());
}

void tft_scrollTo(uint16_t vsp) {
  uint16_t rotation = tft.getRotation();
  //tft.setRotation(0);
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
  //tft.setRotation(rotation);
}


/*
// TODO: Odroid GO, M5Stack
#define _CONFIG_H_ // cancel user config

#define BUTTON_A_PIN 32
#define BUTTON_B_PIN 33

#define BUTTON_MENU 13
#define BUTTON_SELECT 27
#define BUTTON_VOLUME 0
#define BUTTON_START 39
#define BUTTON_JOY_Y 35
#define BUTTON_JOY_X 34

#define SPEAKER_PIN 26
#define TONE_PIN_CHANNEL 0

#define TFT_LED_PIN 5
#define TFT_MOSI 23
#define TFT_MISO 25
#define TFT_SCLK 19
#define TFT_CS 22  // Chip select control pin
#define TFT_DC 21  // Data Command control pin
#define TFT_RST 18  // Reset pin (could connect to Arduino RESET pin)
#include <odroid_go.h>
// #include <M5Stack.h>
//fs::SDFS BLE_FS = SD;

ILI9341 &tft = GO.lcd;

#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
*/

#endif

// UI palette
#define BLE_WHITE       0xFFFF
#define BLE_BLACK       0x0000
#define BLE_GREEN       0x07E0
#define BLE_YELLOW      0xFFE0
#define BLE_GREENYELLOW 0xAFE5
#define BLE_CYAN        0x07FF
#define BLE_ORANGE      0xFD20
#define BLE_DARKGREY    0x7BEF
#define BLE_LIGHTGREY   0xC618
#define BLE_RED         0xF800
#define BLE_DARKGREEN   0x03E0
#define BLE_PURPLE      0x780F
#define BLE_PINK        0xF81F

// top and bottom non-scrolly zones
#define HEADER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
#define FOOTER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
// BLECard info styling
#define IN_CACHE_COLOR tft.color565(0x37, 0x6b, 0x37)
#define NOT_IN_CACHE_COLOR tft.color565(0xa4, 0xa0, 0x5f)
#define ANONYMOUS_COLOR tft.color565(0x88, 0x88, 0x88)
#define NOT_ANONYMOUS_COLOR tft.color565(0xee, 0xee, 0xee)
// one carefully chosen blue
#define BLUETOOTH_COLOR tft.color565(0x14, 0x54, 0xf0)
#define BLE_DARKORANGE tft.color565(0x80, 0x40, 0x00)
// middle scrolly zone
#define BLECARD_BGCOLOR tft.color565(0x22, 0x22, 0x44)
static uint16_t BGCOLOR = tft.color565(0x22, 0x22, 0x44);
