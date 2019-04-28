

/*
 * This D32Pro profile isn't really useful as it's using a 128x128 display
 * and a modified version of the M5Stack core.
 * 
 * It was setup as the worst-case scenario for UI stressing, don't use it
 * unless you have things to test !! 
 * 
 */

//#define USER_SETUP_LOADED WHAAA // this should raise a warning at compilation
//#define TFT_WIDTH 240
//#define TFT_HEIGHT 240
#define ILI9341_VSCRSADD 0x37
#define ILI9341_VSCRDEF 0x33

#define SKIP_INTRO // don't play intro (tft spi access messes up SD/DB init)

#include <FS.h>

#include <TFT_eSPI.h>
#include <JPEGDecoder.h>
TFT_eSPI tft;// = TFT_eSPI(240, 240);


#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
#define BLE_FS_TYPE "sdcard"
#define SD_begin() BLE_FS.begin()

// TODO: make this SD-driver dependant rather than platform dependant
static bool isInQuery() {
  return isQuerying; // M5Stack uses SPI SD, isolate SD accesses from TFT rendering
}

//#define tft_drawJpg tft.drawJpg
#define tft_color565 tft.color565
#define tft_readPixels tft.readRect
#define tft_initOrientation() //tft.setRotation(2)
#define scrollpanel_height() tft.height()
#define scrollpanel_width() tft.width()
#define tft_drawBitmap //tft.pushImage

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


void tft_scrollTo(byte vsp) {
  /*
    int16_t bfa = 0;

    top = 80;
    bfa = 1;
    scrollines = 160;

    Serial.printf("Scrolling top: %d, lines:%d, bottom:%d, vscroll:%d\n", top, scrollines, bfa, vsp);
   
    tft.writecommand(0x33);
    tft.writedata(top >> 8); // TFA
    tft.writedata(top);

    tft.writedata(scrollines >> 8); // VSA
    tft.writedata(scrollines);

    tft.writedata(bfa >> 8); // BFA
    tft.writedata(bfa);
*/
    tft.writecommand(0x37);
    
    tft.writedata(vsp >> 8); // VSP
    tft.writedata(vsp);

}

#define FOOTER_BOTTOMPOS 80
static int bfa, tfa;

void tft_setupScrollArea(byte tfa, byte bfa) {
  uint16_t scrollines = scrollpanel_height() - ( tfa + bfa );

  tfa = 80;
  bfa = 0;
  scrollines = 160;

  tft.writecommand(0x33);
  tft.writedata(tfa >> 8); // TFA
  tft.writedata(tfa);

  tft.writedata(scrollines >> 8); // VSA
  tft.writedata(scrollines);

  tft.writedata(bfa >> 8); // BFA
  tft.writedata(bfa);

  Serial.printf("Init Scroll area with tfa/bfa/vsa %d/%d/%d on w/h %d/%d\n", tfa, bfa, scrollines, scrollpanel_width(), scrollpanel_height());
  
}

/*
void tft_scrollTo(byte vsp) {
  vertScroll(80, 160, vsp);
}*/



/*
void tft_setupScrollArea(byte tfa, byte bfa) {
  tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  tft.writedata(tfa >> 8);           // Top Fixed Area line count
  tft.writedata(tfa);
  tft.writedata((scrollpanel_height()-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  tft.writedata(scrollpanel_height()-tfa-bfa);
  tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
  tft.writedata(bfa);
  log_e("Init Scroll area with tfa/bfa %d/%d on w/h %d/%d", tfa, bfa, scrollpanel_width(), scrollpanel_height());
}

void tft_scrollTo(byte vsp) {
  uint16_t rotation = tft.getRotation();
  //tft.setRotation(0);
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
  //tft.setRotation(rotation);
}
*/

uint32_t jpegRender(int xpos, int ypos) {
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;
  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);
  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = min(mcu_w, max_x % mcu_w);
  uint32_t min_h = min(mcu_h, max_y % mcu_h);
    // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;
  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();
  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;
  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)
    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;
    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;
    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;
    // copy pixels into a contiguous block
    if (win_w != mcu_w) {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }
    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;
    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= tft.height())
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }
  tft.setSwapBytes(swapBytes);
  //showTime(millis() - drawTime); // These lines are for sketch testing only
  return millis() - drawTime;
}


void tft_drawJpg(const uint8_t * jpg_data, size_t jpg_len, uint16_t x=0, uint16_t y=0, uint16_t maxWidth=0, uint16_t maxHeight=0) {
  // Open the named file (the Jpeg decoder library will close it)
  // Use one of the following methods to initialise the decoder:
  boolean decoded = JpegDec.decodeArray(jpg_data, jpg_len);
  //boolean decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)
  if (decoded) {
    jpegRender(x, y);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}
