/*
* LoLin D32 Pro Pins mapping
* IO32 = TFT_LED
* IO33 = TFT_RST
* IO27 = TFT_DC
* IO23 = MOSI
* IO19 = MISO
* IO18 = SCK
* IO14 = TFT_CS
* IO12 = TS_CS // touchscreen
* 
* IO21 = SDA
* IO22 = SCL
* 
#define TF_CS   4  // TF (Micro SD Card) CS pin
#define TS_CS   12 // Touch Screen CS pin
#define TFT_CS  14 // TFT CS pin
#define TFT_LED 32 // TFT backlight control pin
#define TFT_RST 33 // TFT reset pin
#define TFT_DC  27 // TFT DC pin
#define SS      TF_CS
* 
* */


/*
 * This D32Pro profile isn't really useful as it's using a 128x128 display
 * and a modified version of the M5Stack core.
 * 
 * It was setup as the worst-case scenario for UI stressing, don't use it
 * unless you have things to test !! 
 * 
 */

#define USER_SETUP_LOADED WHAAA // this should raise a warning at compilation
#define TFT_WIDTH 128
#define TFT_HEIGHT 128
#define ILI9341_VSCRSADD 0x37
#define ILI9341_VSCRDEF 0x33

#include "D32Pro.h" // a modified version of M5Stack.h with a different LCD profile
//#include <M5StackUpdater.h>

M5Display tft;

#include <SD.h>
fs::SDFS &BLE_FS = SD;
#define BLE_FS_TYPE "sd"
#define SD_begin() BLE_FS.begin()

// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}

#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define tft_initOrientation() tft.setRotation(2)
#define scrollpanel_height() tft.height()
#define scrollpanel_width() tft.width()

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
  return; // working but screen is too small
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
  return; // working but screen is too small
  uint16_t rotation = tft.getRotation();
  //tft.setRotation(0);
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
  //tft.setRotation(rotation);
}
#undef M5STACK
