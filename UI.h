
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

*/

// icon positions for RTC/DB/BLE
#define ICON_APP_X 124
#define ICON_APP_Y 0
#define ICON_RTC_X 92
#define ICON_RTC_Y 7
#define ICON_BLE_X 104
#define ICON_BLE_Y 7
#define ICON_DB_X 116
#define ICON_DB_Y 7
#define ICON_R 4
// blescan progress bar vertical position
#define PROGRESSBAR_Y 30
// sizing the UI
#define HEADER_HEIGHT 40 // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
#define FOOTER_HEIGHT 40 // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
#define SCROLL_HEIGHT ( Out.height - ( HEADER_HEIGHT + FOOTER_HEIGHT ))
// heap map settings
#define HEAPMAP_BUFFLEN 61 // graph width (+ 1 for hscroll)
// heap management (used by graph)
static uint32_t min_free_heap = 90000; // sql out of memory errors eventually occur under 100000
static uint32_t initial_free_heap = freeheap;
static uint32_t heap_tolerance = 20000; // how much memory under min_free_heap the sketch can go and recover without restarting itself
static uint32_t heapmap[HEAPMAP_BUFFLEN] = {0}; // stores the history of heapmap values
static uint16_t heapindex = 0; // index in the circular buffer
static bool blinkit = false; // task blinker state
static bool blinktoggler = true;
static bool appIconVisible = false;
static bool foundTimeServer = false;
static bool gpsIconVisible = false;
static uint16_t blestateicon;
static uint16_t lastblestateicon;
static uint16_t dbIconColor;
static uint16_t lastdbIconColor;
static unsigned long blinknow = millis(); // task blinker start time
static unsigned long scanTime = SCAN_DURATION * 1000; // task blinker duration
static unsigned long blinkthen = blinknow + scanTime; // task blinker end time
static unsigned long lastblink = millis(); // task blinker last blink
static unsigned long lastprogress = millis(); // task blinker progress

const char* sessDevicesCountTpl = " Seen: %4s ";
const char* devicesCountTpl = " Last: %4s ";
const char* newDevicesCountTpl = " Scans:%4s ";
const char* heapTpl = " Heap: %6d";
const char* entriesTpl = " Entries:%4s";
const char* addressTpl = "  %s";
const char* dbmTpl = "%d dBm    ";
const char* ouiTpl = "      %s";
const char* appearanceTpl = "  Appearance: %d";
const char* manufTpl = "      %s";
const char* nameTpl = "      %s";
const char* screenshotFilenameTpl = "/screenshot-%04d-%02d-%02d_%02dh%02dm%02ds.565";
const char* hitsTimeStampTpl = "      %04d/%02d/%02d %02d:%02d:%02d %s";

static char entriesStr[14] = {'\0'};
static char heapStr[16] = {'\0'};
static char sessDevicesCountStr[16] = {'\0'};
static char devicesCountStr[16] = {'\0'};
static char newDevicesCountStr[16] = {'\0'};
static char unitOutput[16] = {'\0'};
static char screenshotFilenameStr[42] = {'\0'};
static char addressStr[24] = {'\0'};
static char dbmStr[16] = {'\0'};
static char hitsTimeStampStr[48] = {'\0'};
static char hitsStr[16] = {'\0'};
static char nameStr[38] = {'\0'};
static char ouiStr[38] = {'\0'};
static char appearanceStr[48] = {'\0'};
static char manufStr[38] = {'\0'};

char *macAddressToColorStr = (char*)calloc(MAC_LEN+1, sizeof(char*));
byte macBytesToBMP[8];
uint16_t macBytesToColors[128];
static uint16_t imgBuffer[320]; // one scan line used for screen capture

uint16_t macAddressToColor( const char *address ) {
  memcpy( macAddressToColorStr, address, MAC_LEN+1);
  byte curs = 0, tokenpos = 0, val, i, j, macpos, msb, lsb;
  char *token;
  char *ptr;
  uint16_t color = 0;
  token = strtok(macAddressToColorStr, ":");
  while(token != NULL) {
    val = strtol(token, &ptr, 16);
    switch( tokenpos )  {
      case 0: msb = val; break;
      case 1: lsb = val; break;
      default: 
        macpos = tokenpos-2;
        macBytesToBMP[macpos] = val; 
        macBytesToBMP[7-macpos]= val;
      break;
    }
    tokenpos++;
    token = strtok(NULL, ":");
  }
  color = (msb*256) + lsb;
  curs = 0;
  for(i=0;i<8;i++) {
    for(j=0;j<8;j++) {
      if( bitRead( macBytesToBMP[j], i ) == 1 ) {
        macBytesToColors[curs++] =  color;
        macBytesToColors[curs++] =  color;
      } else {
        macBytesToColors[curs++] =  BLE_WHITE;
        macBytesToColors[curs++] =  BLE_WHITE;
      }
    }
  }
  return color;
}


static char *formatUnit( int64_t number ) {
  *unitOutput = {'\0'};
  if( number > 999999 ) {
    sprintf(unitOutput, "%dM", number/1000000);
  } else if( number > 999 ) {
    sprintf(unitOutput, "%dK", number/1000);
  } else {
    sprintf(unitOutput, "%d", number);
  }
  return unitOutput;
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



class UIUtils {
  public:

    bool filterVendors = false;

    struct BLECardStyle {
      uint16_t textColor = BLE_WHITE;
      uint16_t borderColor = BLE_WHITE;
      uint16_t bgColor = BLECARD_BGCOLOR;
      void setTheme( BLECardThemes themeID ) {
        bgColor = BLECARD_BGCOLOR;
        switch ( themeID ) {
          case IN_CACHE_ANON:         borderColor = IN_CACHE_COLOR;     textColor = ANONYMOUS_COLOR;     break; // = 0,
          case IN_CACHE_NOT_ANON:     borderColor = IN_CACHE_COLOR;     textColor = NOT_ANONYMOUS_COLOR; break; // = 1,
          case NOT_IN_CACHE_ANON:     borderColor = NOT_IN_CACHE_COLOR; textColor = ANONYMOUS_COLOR;     break; // = 2,
          case NOT_IN_CACHE_NOT_ANON: borderColor = NOT_IN_CACHE_COLOR; textColor = NOT_ANONYMOUS_COLOR; break; // = 3
        }
      }
    };

    BLECardStyle BLECardTheme;

    void init() {

      bool clearScreen = true;
      if (resetReason == 12) { // SW Reset
        clearScreen = false;
      }

      tft.begin();
      tft_initOrientation(); //tft.setRotation( 0 ); // required to get smooth scrolling
      tft.setTextColor(BLE_YELLOW);

      if (clearScreen) {
        tft.fillScreen(BLE_BLACK);
        tft.fillRect(0, HEADER_HEIGHT, Out.width, SCROLL_HEIGHT, BLECARD_BGCOLOR);
        // clear heap map
        for (uint16_t i = 0; i < HEAPMAP_BUFFLEN; i++) heapmap[i] = 0;
      }

      tft.fillRect(0, 0, Out.width, HEADER_HEIGHT, HEADER_BGCOLOR);
      tft.fillRect(0, Out.height - FOOTER_HEIGHT, Out.width, FOOTER_HEIGHT, FOOTER_BGCOLOR);
      tft.fillRect(0, PROGRESSBAR_Y, Out.width, 2, BLE_GREENYELLOW);

      AmigaBall.init();

      alignTextAt( "BLE Collector", 6, 4, BLE_YELLOW, HEADER_BGCOLOR, ALIGN_FREE );
      if (resetReason == 12) { // SW Reset
        headerStats("Rebooted");
      } else {
        headerStats("Init UI");
      }
      Out.setupScrollArea(HEADER_HEIGHT, FOOTER_HEIGHT);
      tft.setTextColor( BLE_WHITE, BLECARD_BGCOLOR );

      SDSetup();
      timeSetup();
      #ifdef NEEDS_SDUPDATER // NTP_MENU and CHRONOMANIAC SD-mirror themselves
        selfReplicateToSD();
      #endif
      timeStateIcon();
      footerStats();
      #ifndef M5STACK
      xTaskCreatePinnedToCore(taskHeapGraph, "taskHeapGraph", 2048, NULL, 2, NULL, 1);
      #endif
      if ( clearScreen ) {
        playIntro();
      } else {
        Out.scrollNextPage();
      }
      
    }


    void update() {
      if ( freeheap + heap_tolerance < min_free_heap ) {
        headerStats("Out of heap..!");
        log_e("[FATAL] Heap too low: %d", freeheap);
        delay(1000);
        ESP.restart();
      }
      headerStats();
      footerStats();
    }


    void playIntro() {
      takeMuxSemaphore();
      uint16_t pos = 0;
      tft.setTextColor(BLE_GREENYELLOW, BGCOLOR);
      for (int i = 0; i < 5; i++) {
        pos += Out.println();
      }
      pos += Out.println("         ");
      pos += Out.println("           ESP32 BLE Collector  ");
      pos += Out.println("         ");
      pos += Out.println("           (c+)  tobozo  2018   ");
      pos += Out.println("         ");
      tft.drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, 106, Out.scrollPosY - pos + 8, 28,  28);
      Out.drawScrollableRoundRect( 58, Out.scrollPosY-pos, 128, pos, 8, BLE_GREENYELLOW );
      for (int i = 0; i < 5; i++) {
        Out.println(SPACE);
      }
      delay(2000);
      Out.scrollNextPage();
      giveMuxSemaphore();
      xTaskCreatePinnedToCore(introUntilScroll, "introUntilScroll", 2048, NULL, 1, NULL, 1);
    }


    static void screenShot() {
      *screenshotFilenameStr = {'\0'};
      sprintf(screenshotFilenameStr, screenshotFilenameTpl, year(), month(), day(), hour(), minute(), second());
      File screenshotFile = BLE_FS.open( screenshotFilenameStr, FILE_WRITE);
      if(!screenshotFile) {
        Serial.printf("Failed to open file %s\n", screenshotFilenameStr);
        screenshotFile.close();
        return;
      }
      takeMuxSemaphore();
      for(uint16_t y=0; y<HEADER_HEIGHT; y++) { // header portion
        tft_readPixels(0, y, Out.width, 1, imgBuffer);
        screenshotFile.write( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
      }
      for(uint16_t y=Out.yStart; y<Out.height-FOOTER_HEIGHT; y++) { // lower scroll portion
        tft_readPixels(0, y, Out.width, 1, imgBuffer);
        screenshotFile.write( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
      }
      for(uint16_t y=HEADER_HEIGHT; y<Out.yStart; y++) { // upper scroll portion
        tft_readPixels(0, y, Out.width, 1, imgBuffer);
        screenshotFile.write( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
      }
      for(uint16_t y=Out.height-FOOTER_HEIGHT; y<Out.height; y++) { // footer portion
        tft_readPixels(0, y, Out.width, 1, imgBuffer);
        screenshotFile.write( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
      }
      giveMuxSemaphore();
      screenshotFile.close();
      Serial.printf("Screenshot saved as %s, now go to http://rawpixels.net/ to decode it (RGB565 240x320 Little Endian)\n", screenshotFilenameStr);
    }

    static void screenShow( void * fileName = NULL ) {
      if( fileName == NULL ) return;
      File screenshotFile = BLE_FS.open( (const char*)fileName );
      if(!screenshotFile) {
        Serial.printf("Failed to open file %s\n", (const char*) fileName);
        screenshotFile.close();
        return;
      }
      takeMuxSemaphore();
      Out.scrollNextPage(); // reset scroll position to zero otherwise image will have offset
      for(uint16_t y=0; y<Out.height; y++) {
        screenshotFile.read( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
        tft.drawBitmap(0, y, Out.width, 1, imgBuffer);
      }
      giveMuxSemaphore();
    }


    static void introUntilScroll( void * param ) {
      int initialscrollPosY = Out.scrollPosY;
      // animate until the scroll is called
      while(initialscrollPosY == Out.scrollPosY) {
        AmigaBall.animate(1, false);
        delay(1);
      }
      vTaskDelete(NULL);
    }


    static void headerStats(const char *status = "") {
      if (isScrolling) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();
      int16_t statuspos = 0;
      *heapStr = {'\0'};
      *entriesStr = {'\0'};
      sprintf(heapStr, heapTpl, freeheap);
      sprintf(entriesStr, entriesTpl, formatUnit(entries));
      alignTextAt( heapStr, 128, 4, BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_RIGHT );
      if ( !isEmpty( status ) ) {
        byte alignoffset = 5;
        tft.fillRect(0, 18, ICON_APP_X, 8, HEADER_BGCOLOR); // clear whole message status area
        if (strstr(status, "Inserted")) {
          tft.drawJpg(disk_jpeg, disk_jpeg_len, alignoffset, 18, 8, 8); // disk icon
          alignoffset +=10;
        } else if (strstr(status, "Cache")) {
          tft.drawJpg(ghost_jpeg, ghost_jpeg_len, alignoffset, 18, 8, 8); // disk icon
          alignoffset +=10;
        } else if (strstr(status, "DB")) {
          tft.drawJpg(moai_jpeg, moai_jpeg_len, alignoffset, 18, 8, 8); // disk icon
          alignoffset +=10;
        } else {
          
        }
        alignTextAt( status, alignoffset, 18, BLE_YELLOW, HEADER_BGCOLOR, ALIGN_FREE );
        statuspos = Out.x1_tmp + Out.w_tmp;
      }
      alignTextAt( entriesStr, 128, 18, BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_RIGHT );
      tft.drawJpg(ram_jpeg, ram_jpeg_len, 156, 4, 8, 8); // heap icon
      tft.drawJpg(earth_jpeg, earth_jpeg_len, 156, 18, 8, 8); // entries icon

      if( !appIconVisible || statuspos > ICON_APP_X ) { // only draw if text has overlapped
        tft.drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, ICON_APP_X, ICON_APP_Y, 28,  28); // app icon
        appIconVisible = true;
      }
      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void footerStats() {
      if (isScrolling) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();


/*
tft.height() - 32; // 288
tft.height() - 22; // 298
tft.height() - 12; // 208

      HHMM_POS_Y 288
      UPTIME_POS_Y 298
      COPYLEFT_POS_Y 308
*/
      alignTextAt( hhmmString,   85, Out.height - 32/*288*/, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt( UpTimeString, 85, Out.height - 22/*298*/, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt("(c+) tobozo", 77, Out.height - 12/*308*/, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      if( gpsIconVisible ) {
        tft.drawJpg( gps_jpg, gps_jpg_len, 128, Out.height - 34/*286*/, 10,  10);
      } else {
        tft.fillRect( 128, Out.height - 34/*286*/, 10,  10, FOOTER_BGCOLOR );
      }
      *sessDevicesCountStr = {'\0'};
      *devicesCountStr = {'\0'};
      *newDevicesCountStr = {'\0'};

      sprintf( sessDevicesCountStr, sessDevicesCountTpl, formatUnit(sessDevicesCount) );
      sprintf( devicesCountStr, devicesCountTpl, formatUnit(devicesCount) );
      sprintf( newDevicesCountStr, newDevicesCountTpl, formatUnit(scan_rounds) );

      alignTextAt( devicesCountStr, 0, Out.height - 32/*288*/, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );
      alignTextAt( sessDevicesCountStr, 0, Out.height - 22/*298*/, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );
      alignTextAt( newDevicesCountStr, 0, Out.height - 12/*308*/, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );

      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void cacheStats() {
      takeMuxSemaphore();
      percentBox(164, Out.height - 36/*284*/, 10, 10, BLEDevCacheUsed, BLE_CYAN, BLE_BLACK);
      percentBox(164, Out.height - 24/*296*/, 10, 10, VendorCacheUsed, BLE_ORANGE, BLE_BLACK);
      percentBox(164, Out.height - 12/*308*/, 10, 10, OuiCacheUsed, BLE_GREENYELLOW, BLE_BLACK);
      if( filterVendors ) {
        tft.drawJpg( filter_jpeg, filter_jpeg_len, 152, Out.height - 12/*308*/, 10,  8);
      } else {
        tft.fillRect( 152, Out.height - 12/*308*/, 10,  8, FOOTER_BGCOLOR );
      }
      giveMuxSemaphore();
    }


    void percentBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t percent, uint16_t barcolor, uint16_t bgcolor, uint16_t bordercolor = BLE_DARKGREY) {
      if (percent == 0) {
        tft.drawRect(x - 1, y - 1, w + 2, h + 2, bordercolor);
        tft.fillRect(x, y, w, h, bgcolor);
        return;
      }
      if (percent == 100) {
        tft.drawRect(x - 1, y - 1, w + 2, h + 2, BLUETOOTH_COLOR);
        tft.fillRect(x, y, w, h, barcolor);
        return;
      }
      tft.drawRect(x - 1, y - 1, w + 2, h + 2, bordercolor);
      tft.fillRect(x, y, w, h, bgcolor);
      byte yoffsetpercent = percent / 10;
      byte boxh = (yoffsetpercent * h) / 10 ;
      tft.fillRect(x, y, w, boxh, barcolor);

      byte xoffsetpercent = percent % 10;
      if (xoffsetpercent == 0) return;
      byte linew = (xoffsetpercent * w) / 10;
      tft.drawFastHLine(x, y + boxh, linew, barcolor);
    }


    static void timeStateIcon() {
      tft.drawJpg( clock_jpeg, clock_jpeg_len, ICON_RTC_X-ICON_R, ICON_RTC_Y-ICON_R+1, 8,  8);
      //tft.fillCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_GREENYELLOW);
      if (RTCisRunning) {
        //tft.drawCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_DARKGREEN);
        //tft.drawFastHLine(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_DARKGREY);
        //tft.drawFastVLine(ICON_RTC_X, ICON_RTC_Y, ICON_R - 2, BLE_DARKGREY);
      } else {
        tft.drawCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_RED);
        //tft.drawFastHLine(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_RED);
        //tft.drawFastVLine(ICON_RTC_X, ICON_RTC_Y, ICON_R - 2, BLE_RED);
      }
    }


    static void startBlink() { // runs one and detaches
      tft.drawJpg( zzz_jpeg, zzz_jpeg_len, 5, 18, 8,  8 );
      blinkit = true;
      blinknow = millis();
      scanTime = SCAN_DURATION * 1000;
      blinkthen = blinknow + scanTime;
      lastblink = millis();
      lastprogress = millis();
    }


    static void stopBlink() {
      // clear progress bar
      blinkit = false;
      delay(150); // give some time to the task to end
    }

    // sqlite state (read/write/inert) icon
    static void DBStateIconSetColor(int state) {
      switch (state) {
        case 2:/*DB OPEN FOR WRITING*/ dbIconColor = BLE_ORANGE;    break;
        case 1:/*DB_OPEN FOR READING*/ dbIconColor = BLE_YELLOW;    break;
        case 0:/*DB CLOSED*/           dbIconColor = BLE_DARKGREEN; break;
        case -1:/*DB BROKEN*/          dbIconColor = BLE_RED;       break;
        default:/*DB INACTIVE*/        dbIconColor = BLE_DARKGREY;  break;
      }
    }

    static void BLEStateIconSetColor(uint16_t color) {
      blestateicon = color;
    }

    static void PrintProgressBar(uint16_t width) {
      if( width == Out.width ) {
        tft.fillRect(0, PROGRESSBAR_Y, width, 2, BLE_DARKGREY);
      } else {
        tft.fillRect(0, PROGRESSBAR_Y, width, 2, BLUETOOTH_COLOR);
      }
    }


    static void PrintDBStateIcon() {
      if( lastdbIconColor != dbIconColor ) {
        log_v("blinked at %d", dbIconColor);
        takeMuxSemaphore();
        tft.fillCircle(ICON_DB_X, ICON_DB_Y, ICON_R, dbIconColor);
        giveMuxSemaphore();
        lastdbIconColor = dbIconColor;
      }
    }


    static void PrintBLEStateIcon(bool fill = true) {
      if( lastblestateicon != blestateicon ) {
        takeMuxSemaphore();
        lastblestateicon = blestateicon;
        if (fill) {
          tft.fillCircle(ICON_BLE_X, ICON_BLE_Y, ICON_R, blestateicon);
          if( foundTimeServer ) {
            tft.drawCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_GREEN);
          }
        } else {
          if( foundTimeServer ) {
            tft.drawCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, BLE_ORANGE);
          }
          tft.fillCircle(ICON_BLE_X, ICON_BLE_Y, ICON_R - 1, blestateicon);
        }
        if( !foundTimeServer ) {
          tft.drawCircle(ICON_RTC_X, ICON_RTC_Y, ICON_R, HEADER_BGCOLOR);
          tft.drawJpg( clock_jpeg, clock_jpeg_len, ICON_RTC_X-ICON_R, ICON_RTC_Y-ICON_R+1, 8,  8);
        }
        giveMuxSemaphore();
      }
    }


    static void PrintBleScanWidgets() {
      if (!blinkit || blinknow >= blinkthen) {
        blinkit = false;
        if(blestateicon!=BLE_DARKGREY) {
          takeMuxSemaphore();
          //tft.fillRect(0, PROGRESSBAR_Y, Out.width, 2, BLE_DARKGREY);
          PrintProgressBar( Out.width );
          giveMuxSemaphore();
          // clear blue pin
          BLEStateIconSetColor(BLE_DARKGREY);
        }
        return;
      }

      blinknow = millis();
      if (lastblink + random(222, 666) < blinknow) {
        blinktoggler = !blinktoggler;
        if (blinktoggler) {
          BLEStateIconSetColor(BLUETOOTH_COLOR);
        } else {
          BLEStateIconSetColor(HEADER_BGCOLOR);
        }
        lastblink = blinknow;
      }

      if (lastprogress + 1000 < blinknow) {
        unsigned long remaining = blinkthen - blinknow;
        int percent = 100 - ( ( remaining * 100 ) / scanTime );
        takeMuxSemaphore();
        PrintProgressBar( (Out.width * percent) / 100 );
        //tft.fillRect(0, PROGRESSBAR_Y, (Out.width * percent) / 100, 2, BLUETOOTH_COLOR);
        giveMuxSemaphore();
        lastprogress = blinknow;
      }
    }

    // spawn subtasks and leave
    static void taskHeapGraph( void * pvParameters ) { // always running
      mux = xSemaphoreCreateMutex();
      xTaskCreatePinnedToCore(heapGraph, "HeapGraph", 4096, NULL, 4, NULL, 0); /* last = Task Core */
      #if HAS_EXTERNAL_RTC
      xTaskCreatePinnedToCore(clockSync, "clockSync", 2048, NULL, 4, NULL, 1); // RTC wants to run on core 1 or it fails
      #endif
      vTaskDelete(NULL);
    }


    static void clockSync(void * parameter) {
      unsigned long lastClockTick = millis();
      while(1) {
        if(lastClockTick + 1000 > millis()) {
          vTaskDelay( 100 );
          continue;
        }
        #if HAS_EXTERNAL_RTC
        takeMuxSemaphore();
        timeHousekeeping();
        giveMuxSemaphore();
        #endif
        lastClockTick = millis();
        #if HAS_GPS
          if( GPSHasDateTime ) {
            gpsIconVisible = true;          
          } else {
            gpsIconVisible = false;
          }
        #endif
        
        vTaskDelay( 100 );
      }
    }


    static void heapGraph(void * parameter) {
      uint32_t lastfreeheap;
      uint32_t toleranceheap = min_free_heap + heap_tolerance;
      uint8_t i = 0;
      int16_t GRAPH_LINE_WIDTH = HEAPMAP_BUFFLEN - 1;
      int16_t GRAPH_LINE_HEIGHT = 35;
      int16_t GRAPH_X = Out.width - GRAPH_LINE_WIDTH - 2;
      int16_t GRAPH_Y = Out.height - 37/*283*/;

      while (1) {

        if (isScrolling) {
          vTaskDelay( 10 );
          continue;
        }
        // do the blinky stuff
        PrintDBStateIcon();
        PrintBLEStateIcon();
        PrintBleScanWidgets();
        // see if heatmap needs updating
        if (lastfreeheap != freeheap) {
          heapmap[heapindex++] = freeheap;
          heapindex = heapindex % HEAPMAP_BUFFLEN;
          lastfreeheap = freeheap;
        } else {
          vTaskDelay( 10 );
          continue;
        }
        // render heatmap
        takeMuxSemaphore();
        uint16_t GRAPH_COLOR = BLE_WHITE;
        uint32_t graphMin = min_free_heap;
        uint32_t graphMax = graphMin;
        uint32_t toleranceline = GRAPH_LINE_HEIGHT;
        uint32_t minline = 0;
        uint16_t GRAPH_BG_COLOR = BLE_BLACK;
        // dynamic scaling
        for (i = 0; i < GRAPH_LINE_WIDTH; i++) {
          int thisindex = int(heapindex - GRAPH_LINE_WIDTH + i + HEAPMAP_BUFFLEN) % HEAPMAP_BUFFLEN;
          uint32_t heapval = heapmap[thisindex];
          if (heapval != 0 && heapval < graphMin) {
            graphMin =  heapval;
          }
          if (heapval > graphMax) {
            graphMax = heapval;
          }
        }

        if (graphMin == graphMax) {
          // data isn't relevant enough to render
          giveMuxSemaphore();
          vTaskDelay( 100 );
          continue; 
        }
        // bounds, min and max lines
        minline = map(min_free_heap, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
        if (toleranceheap > graphMax) {
          GRAPH_BG_COLOR = BLE_ORANGE;
          toleranceline = GRAPH_LINE_HEIGHT;
        } else if ( toleranceheap < graphMin ) {
          toleranceline = 0;
        } else {
          toleranceline = map(toleranceheap, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
        }
        // draw graph
        for (i = 0; i < GRAPH_LINE_WIDTH; i++) {
          int thisindex = int(heapindex - GRAPH_LINE_WIDTH + i + HEAPMAP_BUFFLEN) % HEAPMAP_BUFFLEN;
          uint32_t heapval = heapmap[thisindex];
          if ( heapval > toleranceheap ) {
            // nominal, all green
            GRAPH_COLOR = BLE_GREEN;
            GRAPH_BG_COLOR = BLE_DARKGREY;
          } else {
            if ( heapval > min_free_heap ) {
              // in tolerance zone
              GRAPH_COLOR = BLE_YELLOW;
              GRAPH_BG_COLOR = BLE_DARKGREEN;
            } else {
              // under tolerance zone
              if (heapval > 0) {
                GRAPH_COLOR = BLE_RED;
                GRAPH_BG_COLOR = BLE_ORANGE;
              } else {
                // no record
                GRAPH_BG_COLOR = BLE_BLACK;
              }
            }
          }
          // fill background
          tft.drawFastVLine( GRAPH_X + i, GRAPH_Y, GRAPH_LINE_HEIGHT, GRAPH_BG_COLOR );
          if ( heapval > 0 ) {
            uint32_t lineheight = map(heapval, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
            tft.drawFastVLine( GRAPH_X + i, GRAPH_Y + GRAPH_LINE_HEIGHT-lineheight, lineheight, GRAPH_COLOR );
          }
        }
        tft.drawFastHLine( GRAPH_X, GRAPH_Y + GRAPH_LINE_HEIGHT - toleranceline, GRAPH_LINE_WIDTH, BLE_LIGHTGREY );
        tft.drawFastHLine( GRAPH_X, GRAPH_Y + GRAPH_LINE_HEIGHT - minline, GRAPH_LINE_WIDTH, BLE_RED );
        giveMuxSemaphore();
        vTaskDelay( 100 );
      }
    }


    void printBLECard( BlueToothDevice *BleCard ) {
      // don't render if already on screen
      if( BLECardIsOnScreen( BleCard->address ) ) {
        log_d("%s is already on screen, skipping rendering", BleCard->address);
        return;
      }

      if ( isEmpty( BleCard->address ) ) {
        log_w("Cowardly refusing to render %d with an empty address", 0);
        return;
      }

      if( filterVendors ) {
        if(strcmp( BleCard->ouiname, "[random]")==0 ) {
          log_i("Filtering %s with random vendorname", BleCard->address);
          return;
        }
      }

      log_d("  [printBLECard] %s will be rendered", BleCard->address);

      takeMuxSemaphore();

      //MacScrollView
      uint16_t randomcolor = tft.color565( random(128, 255), random(128, 255), random(128, 255) );
      uint16_t blockHeight = 0;
      uint16_t hop;
      uint16_t initialPosY = Out.scrollPosY;
      
      if( BleCard->in_db ) {
        if( BleCard->is_anonymous ) {
          BLECardTheme.setTheme( IN_CACHE_ANON );
        } else {
          BLECardTheme.setTheme( IN_CACHE_NOT_ANON );
        }
      } else {
        if( BleCard->is_anonymous ) {
          BLECardTheme.setTheme( NOT_IN_CACHE_ANON );
        } else {
          BLECardTheme.setTheme( NOT_IN_CACHE_ANON );
        }
      }
      tft.setTextColor( BLECardTheme.textColor, BLECardTheme.bgColor );
      
      BGCOLOR = BLECardTheme.bgColor;
      hop = Out.println( SPACE );
      blockHeight += hop;
      
      *addressStr = {'\0'};
      sprintf( addressStr, addressTpl, BleCard->address );
      hop = Out.println( addressStr );
      blockHeight += hop;

      //if ( !isEmpty( BleCard->ouiname ) && strcmp( BleCard->ouiname, "[random]" )!=0 ) {
        uint16_t macColor = macAddressToColor( (const char*) BleCard->address );
        tft.drawBitmap( 150, Out.scrollPosY - hop, 16, 8, macBytesToColors );
        //maclastbytes
        //tft.setTextColor( macColor, BLE_WHITE /*BLECardTheme.bgColor */);
      //}

      *dbmStr = {'\0'};
      sprintf( dbmStr, dbmTpl, BleCard->rssi );
      alignTextAt( dbmStr, 0, Out.scrollPosY - hop, BLECardTheme.textColor, BLECardTheme.bgColor, ALIGN_RIGHT );
      tft.setCursor( 0, Out.scrollPosY );
      drawRSSI( Out.width - 18, Out.scrollPosY - hop - 1, BleCard->rssi, BLECardTheme.textColor );
      if ( BleCard->in_db ) { // 'already seen this' icon
        tft.drawJpg( update_jpeg, update_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      } else { // 'just inserted this' icon
        tft.drawJpg( insert_jpeg, insert_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      }
      if ( !isEmpty( BleCard->uuid ) ) { // 'has service UUID' Icon
        tft.drawJpg( service_jpeg, service_jpeg_len, 128, Out.scrollPosY - hop, 8,  8);
      }

      switch( BleCard->hits ) {
        case 0:
          tft.drawJpg( ghost_jpeg, ghost_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
        break;
        case 1:
          tft.drawJpg( moai_jpeg, moai_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
        break;
        default:
          tft.drawJpg( disk_jpeg, disk_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
        break;
      }

      if( TimeIsSet ) {
        if ( BleCard->hits > 1 ) {
          *hitsStr = {'\0'};
          sprintf(hitsStr, "(%s hits)", formatUnit( BleCard->hits ) );
  
          if( BleCard->updated_at.unixtime() > 0 /* BleCard->created_at.year() > 1970 */) {
            unsigned long age_in_seconds = abs( BleCard->created_at.unixtime() - BleCard->updated_at.unixtime() );
            unsigned long age_in_minutes = age_in_seconds / 60;
            unsigned long age_in_hours   = age_in_minutes / 60;
            unsigned long seconds_since_boot = (millis() / 1000)+1;
            float freq = ((float)BleCard->hits / (float)scan_rounds+1) * ((float)age_in_seconds / (float)seconds_since_boot+1);
            /*
            Serial.printf(" C: %d, U:%d, (%d / %d) * (%d / %d) = ",
              BleCard->created_at.unixtime(),
              BleCard->updated_at.unixtime(),
              BleCard->hits,
              scan_rounds+1,
              age_in_seconds+1,
              millis() / 1000
            );
            Serial.println( freq * 1000 );*/
          }

          if( BleCard->created_at.year() > 1970 ) {
            blockHeight += Out.println(SPACE);
            *hitsTimeStampStr = {'\0'};
            sprintf(hitsTimeStampStr, hitsTimeStampTpl, 
              BleCard->created_at.year(),
              BleCard->created_at.month(),
              BleCard->created_at.day(),
              BleCard->created_at.hour(),
              BleCard->created_at.minute(),
              BleCard->created_at.second(),
              hitsStr
            );
            hop = Out.println( hitsTimeStampStr );
            blockHeight += hop;
    
            tft.drawJpg( clock_jpeg, clock_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
          }
          
        }
      }

      if ( !isEmpty( BleCard->ouiname ) ) {
        blockHeight += Out.println( SPACE );
        *ouiStr = {'\0'};
        sprintf( ouiStr, ouiTpl, BleCard->ouiname );
        hop = Out.println( ouiStr );
        blockHeight += hop;
        
        if ( strstr( BleCard->ouiname, "Espressif" ) ) {
          tft.drawJpg( espressif_jpeg  , espressif_jpeg_len, 11, Out.scrollPosY - hop, 8, 8 );
        } else {
          tft.drawJpg( nic16_jpeg, nic16_jpeg_len, 10, Out.scrollPosY - hop, 13, 8 );
        }
        
      }
      
      if ( BleCard->appearance != 0 ) {
        blockHeight += Out.println(SPACE);
        *appearanceStr = {'\0'};
        sprintf( appearanceStr, appearanceTpl, BleCard->appearance );
        hop = Out.println( appearanceStr );
        blockHeight += hop;
      }
      if ( !isEmpty( BleCard->manufname ) ) {
        blockHeight += Out.println(SPACE);
        *manufStr = {'\0'};
        sprintf( manufStr, manufTpl, BleCard->manufname );
        hop = Out.println( manufStr );
        blockHeight += hop;
        if ( strstr( BleCard->manufname, "Apple" ) ) {
          tft.drawJpg( apple16_jpeg, apple16_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( BleCard->manufname, "IBM" ) ) {
          tft.drawJpg( ibm8_jpg, ibm8_jpg_len, 10, Out.scrollPosY - hop, 20,  8);
        } else if ( strstr (BleCard->manufname, "Microsoft" ) ) {
          tft.drawJpg( crosoft_jpeg, crosoft_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( BleCard->manufname, "Bose" ) ) {
          tft.drawJpg( speaker_icon_jpg, speaker_icon_jpg_len, 12, Out.scrollPosY - hop, 6,  8 );
        } else {
          tft.drawJpg( generic_jpeg, generic_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        }
      }
      if ( !isEmpty( BleCard->name ) ) {
        *nameStr = {'\0'};
        sprintf(nameStr, nameTpl, BleCard->name);
        blockHeight += Out.println(SPACE);
        hop = Out.println( nameStr );
        blockHeight += hop;
        tft.drawJpg( name_jpeg, name_jpeg_len, 12, Out.scrollPosY - hop, 7,  8);
      }
      hop = Out.println( SPACE) ;
      blockHeight += hop;
      uint16_t boxHeight = blockHeight-2;
      uint16_t boxWidth  = Out.width - 2;
      uint16_t boxPosY   = initialPosY + 1;
      Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, boxHeight, 4, BLECardTheme.borderColor );
      lastPrintedMacIndex++;
      lastPrintedMacIndex = lastPrintedMacIndex % BLECARD_MAC_CACHE_SIZE;
      memcpy( MacScrollView[lastPrintedMacIndex].address, BleCard->address, MAC_LEN+1 );
      MacScrollView[lastPrintedMacIndex].blockHeight = blockHeight;
      MacScrollView[lastPrintedMacIndex].scrollPosY  = boxPosY;//Out.scrollPosY;
      MacScrollView[lastPrintedMacIndex].borderColor = BLECardTheme.borderColor;
      giveMuxSemaphore();
    }


    static bool BLECardIsOnScreen( const char* address ) {
      log_v("Checking if %s is visible onScreen", address);
      uint16_t card_index;
      int16_t offset = 0;
      for(uint16_t i = lastPrintedMacIndex+BLECARD_MAC_CACHE_SIZE; i>lastPrintedMacIndex; i--) {
        card_index = i%BLECARD_MAC_CACHE_SIZE;
        offset+=MacScrollView[card_index].blockHeight;
        if ( strcmp( address, MacScrollView[card_index].address ) == 0 ) {
          if( offset <= Out.yArea ) {
            highlighbBLECard( card_index, -offset );
            log_v("%s is onScreen", address);
            return true;
          } else {
            log_v("%s is in cache but NOT visible onScreen", address);
            return false;
          }
        }
      }
      log_v("%s is NOT in cache and NOT visible onScreen", address);
      return false;      
    }

    static void highlighbBLECard( uint16_t card_index, int16_t offset ) {
      if( card_index >= BLECARD_MAC_CACHE_SIZE) return; // bad value
      if( isEmpty( MacScrollView[card_index].address ) ) return; // empty slot
      int newYPos = Out.translate( Out.scrollPosY, offset );
      headerStats( MacScrollView[card_index].address );
      takeMuxSemaphore();
      uint16_t boxHeight = MacScrollView[card_index].blockHeight-2;
      uint16_t boxWidth  = Out.width - 2;
      uint16_t boxPosY   = newYPos + 1;
      for( int16_t color=255; color>64; color-- ) {
        Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, boxHeight, 4, tft.color565(color, color, color) );
        delay(8); // TODO: use a timer
      }
      Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, MacScrollView[card_index].blockHeight-2, 4, MacScrollView[card_index].borderColor );
      giveMuxSemaphore();
    }


  private:

    static void alignTextAt(const char* text, uint16_t x, uint16_t y, int16_t color = BLE_YELLOW, int16_t bgcolor = BGCOLOR, byte textAlign = ALIGN_FREE) {
      tft.setTextColor(color, bgcolor);
      tft_getTextBounds(text, x, y, &Out.x1_tmp, &Out.y1_tmp, &Out.w_tmp, &Out.h_tmp);
      switch (textAlign) {
        case ALIGN_FREE:
          tft.setCursor(x, y);
          break;
        case ALIGN_LEFT:
          tft.setCursor(0, y);
          break;
        case ALIGN_RIGHT:
          tft.setCursor(Out.width - Out.w_tmp, y);
          break;
        case ALIGN_CENTER:
          tft.setCursor(Out.width / 2 - Out.w_tmp / 2, y);
          break;
      }
      tft.print(text);
    }

    // draws a RSSI Bar for the BLECard
    void drawRSSI(int16_t x, int16_t y, int16_t rssi, uint16_t bgcolor) {
      uint16_t barColors[4];
      if (rssi >= -30) {
        // -30 dBm and more Amazing    - Max achievable signal strength. The client can only be a few feet
        // from the AP to achieve this. Not typical or desirable in the real world.  N/A
        barColors[0] = BLE_GREEN;
        barColors[1] = BLE_GREEN;
        barColors[2] = BLE_GREEN;
        barColors[3] = BLE_GREEN;
      } else if (rssi >= -67) {
        // between -67 dBm and 31 dBm  - Very Good   Minimum signal strength for applications that require
        // very reliable, timely delivery of data packets.   VoIP/VoWiFi, streaming video
        barColors[0] = BLE_GREEN;
        barColors[1] = BLE_GREEN;
        barColors[2] = BLE_GREEN;
        barColors[3] = bgcolor;
      } else if (rssi >= -70) {
        // between -70 dBm and -68 dBm - Okay  Minimum signal strength for reliable packet delivery.   Email, web
        barColors[0] = BLE_YELLOW;
        barColors[1] = BLE_YELLOW;
        barColors[2] = BLE_YELLOW;
        barColors[3] = bgcolor;
      } else if (rssi >= -80) {
        // between -80 dBm and -71 dBm - Not Good  Minimum signal strength for basic connectivity. Packet delivery may be unreliable.  N/A
        barColors[0] = BLE_YELLOW;
        barColors[1] = BLE_YELLOW;
        barColors[2] = bgcolor;
        barColors[3] = bgcolor;
      } else if (rssi >= -90) {
        // between -90 dBm and -81 dBm - Unusable  Approaching or drowning in the noise floor. Any functionality is highly unlikely.
        barColors[0] = BLE_RED;
        barColors[1] = bgcolor;
        barColors[2] = bgcolor;
        barColors[3] = bgcolor;
      }  else {
        // dude, this sucks
        barColors[0] = BLE_RED; // want: BLE_RAINBOW
        barColors[1] = bgcolor;
        barColors[2] = bgcolor;
        barColors[3] = bgcolor;
      }
      tft.fillRect(x,     y + 4, 2, 4, barColors[0]);
      tft.fillRect(x + 3, y + 3, 2, 5, barColors[1]);
      tft.fillRect(x + 6, y + 2, 2, 6, barColors[2]);
      tft.fillRect(x + 9, y + 1, 2, 7, barColors[3]);
    }
};


UIUtils UI;
