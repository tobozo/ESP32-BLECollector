
#include "Free_Fonts.h"
#include <M5Stack.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater

#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE ) || defined( ARDUINO_ODROID_ESP32 ) || defined ( ARDUINO_ESP32_DEV )
  #define CHIMERA_CORE
#else
  #error "NO SUPPORTED BOARD DETECTED !!"
  #error "Please select the right board from the Arduino boards menu"
  #error "Supported boards are: ARDUINO_M5Stack_Core_ESP32, ARDUINO_M5STACK_FIRE, ARDUINO_ODROID_ESP32 or ARDUINO_ESP32_DEV"
  // #define D32PRO // deprecated
  // #define DDUINO32XS // deprecated
#endif

#define BLE_FS M5STACK_SD // inherited from ESP32-Chimera-Core
#define tft M5.Lcd // syntax sugar
#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define scrollpanel_height() tft.width()
#define scrollpanel_width() tft.height()
#define tft_initOrientation() tft.setRotation(1)
#define tft_drawBitmap tft.drawBitmap
#define SD_begin BLE_FS.begin
#define hasHID() (bool)true
#define BLE_FS_TYPE "sd" // sd = fs::SD, sdcard = fs::SD_MMC
#define SKIP_INTRO // don't play intro (tft spi access messes up SD/DB init)
static const int AMIGABALL_YPOS = 50;

// display profiles switcher

#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE ) || defined( ARDUINO_ODROID_ESP32 )

  // custom M5Stack/Odroid-Go go TFT/SD/RTC/GPS settings here (see ARDUINO_ESP32_DEV profile for available settings)

#elif defined ( ARDUINO_ESP32_DEV )

  // since C macros are lazy, overwrite the settings.h values
  #undef HAS_EXTERNAL_RTC
  #undef HAS_GPS
  #undef hasHID
  #undef SD_begin
  #undef scrollpanel_height
  #undef scrollpanel_width
  #undef tft_initOrientation
  #undef RTC_SDA
  #undef RTC_SCL
  #undef GPS_RX
  #undef GPS_TX
  #undef BLE_FS_TYPE

  #define HAS_EXTERNAL_RTC true // will use RTC_SDA and RTC_SCL from settings.h
  #define HAS_GPS true // will use GPS_RX and GPS_TX from settings.h
  #define hasHID() (bool)false // disable buttons
  #define SD_begin() (bool)true // SD_MMC is auto started
  #define tft_initOrientation() tft.setRotation(0) // default orientation for hardware scroll
  #define scrollpanel_height() tft.height() // invert these if scroll fails
  #define scrollpanel_width() tft.width() // invert these if scroll fails
  #define RTC_SDA 26 // pin number
  #define RTC_SCL 27 // pin number
  #define GPS_RX 39 // io pin number
  #define GPS_TX 35 // io pin number
  #define BLE_FS_TYPE "sdcard" // sd = fs::SD, sdcard = fs::SD_MMC

  #warning WROVER KIT DETECTED !!
  
#else

  #error "NO SUPPORTED BOARD DETECTED !!"
  #error "Either define one of the supported boards, add it to the profile selection, or uncomment this error to experiment"

#endif

// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}



void tft_begin() {
  M5.begin( true, true, false, false, false ); // don't start Serial
  delay( 100 );
  M5.ScreenShot.init( &tft, BLE_FS );
  M5.ScreenShot.begin();
  if( hasHID() ) {
    // build has buttons => enable SD Updater at boot
    if(digitalRead(BUTTON_A_PIN) == 0) {
      Serial.println("Will Load menu binary");
      updateFromFS();
      ESP.restart();
    }
  }
}


// emulating Adafruit's tft.getTextBounds()
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
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
}

TFT_eSprite sprite = TFT_eSprite( &tft );

void tft_fillGradientHRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor colorstart, RGBColor colorend ) {
  sprite.setPsram( false ); // don't bother using psram for that
  sprite.setSwapBytes( false );
  sprite.createSprite( width, 1);
  sprite.setColorDepth( 16 );
  sprite.drawGradientHLine( 0, 0, width, colorstart, colorend );
  for( uint16_t h = 0; h < height; h++ ) {
    sprite.pushSprite( x, y+h );
  }
  sprite.deleteSprite();
}

void tft_fillGradientVRect( uint16_t x, uint16_t y, uint16_t width, uint16_t height, RGBColor colorstart, RGBColor colorend ) {
  sprite.setPsram( false ); // don't bother using psram for that
  sprite.setSwapBytes( false );
  sprite.createSprite( 1, height);
  sprite.setColorDepth( 16 );
  sprite.drawGradientVLine( 0, 0, height, colorstart, colorend );
  for( uint16_t w = 0; w < width; w++ ) {
    sprite.pushSprite( x+w, y );
  }
  sprite.deleteSprite();
}

void tft_drawGradientHLine( uint32_t x, uint32_t y, uint32_t w, RGBColor colorstart, RGBColor colorend ) {
  tft_fillGradientHRect( x, y, w, 1, colorstart, colorend );
}

void tft_drawGradientVLine( uint32_t x, uint32_t y, uint32_t h, RGBColor colorstart, RGBColor colorend ) {
  tft_fillGradientVRect( x, y, 1, h, colorstart, colorend );
}
