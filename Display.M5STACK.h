
#include <M5Stack.h>
#include <M5StackUpdater.h>

M5Display tft;

#include <SD.h>
fs::SDFS &BLE_FS = SD;
#define BLE_FS_TYPE "sd"
#define SD_begin() BLE_FS.begin()

// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}

#define SKIP_INTRO // don't play intro (tft spi access messes up SD/DB init)

#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define tft_initOrientation() tft.setRotation(1)
#define scrollpanel_height() tft.width()
#define scrollpanel_width() tft.height()
#define tft_drawBitmap tft.drawBitmap

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
