
#error "DEPRECATED!! THIS DRIVER IS NO LONGER SUPPORTED, USE CHIMERA_CORE INSTEAD"

#include <FS.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include "WROVER_KIT_LCD.h" // Latest version must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
WROVER_KIT_LCD tft;

//#define AMIGABALL_YPOS 150


#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
#define BLE_FS_TYPE "sdcard"
#define SD_begin() BLE_FS.begin()
// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return false; // wrover uses SD_MMC, no need to isolate SPI transactions with semaphores
}

#define tft_begin tft.begin
#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_setupScrollArea tft.setupScrollArea
#define tft_scrollTo tft.scrollTo
#define tft_getTextBounds tft.getTextBounds
#define tft_readPixels tft.readPixels
#define tft_initOrientation() tft.setRotation(0)
#define scrollpanel_height() tft.height()
#define scrollpanel_width() tft.width()
#define tft_drawBitmap tft.drawBitmap
