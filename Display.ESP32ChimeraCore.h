
#include <M5Stack.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h>

#define tft M5.Lcd

#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define scrollpanel_height() tft.width()
#define scrollpanel_width() tft.height()
#define tft_initOrientation() tft.setRotation(1)
#define tft_drawBitmap tft.drawBitmap
#define SD_begin BLE_FS.begin
#define hasHID() (bool)true

static const int AMIGABALL_YPOS = 50;
#define BLE_FS M5STACK_SD // inherited from ESP32-Chimera-Core

#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE ) || defined( ARDUINO_ODROID_ESP32 )

  #define BLE_FS_TYPE "sd"

#elif defined ( ARDUINO_ESP32_DEV )

  #undef HAS_EXTERNAL_RTC
  #undef HAS_GPS
  #undef hasHID
  #undef SD_begin
  #undef scrollpanel_height
  #undef scrollpanel_width
  #undef tft_initOrientation

  #define HAS_EXTERNAL_RTC true
  #define HAS_GPS true
  #define hasHID() (bool)false
  #define SD_begin() (bool)true
  #define tft_initOrientation() tft.setRotation(0)
  #define scrollpanel_height() tft.height()
  #define scrollpanel_width() tft.width()
  
  #define BLE_FS_TYPE "sdcard"

  #warning WROVER KIT DETECTED !!
  
#else
  #warning NOTHING DETECTED !!
#endif

// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}

#define SKIP_INTRO // don't play intro (tft spi access messes up SD/DB init)

void tft_begin() {
  M5.begin( true, true, false, false, false ); // don't start Serial
  delay( 100 );
  M5.ScreenShot.init( &tft, BLE_FS );
  M5.ScreenShot.begin();
  if( hasHID() ) {
    // have buttons => enable SD Updater
    if(digitalRead(BUTTON_A_PIN) == 0) {
      Serial.println("Will Load menu binary");
      updateFromFS();
      ESP.restart();
    }
  }
}

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
