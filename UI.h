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
#define GRAPH_LINE_WIDTH HEAPMAP_BUFFLEN - 1
#define GRAPH_LINE_HEIGHT 35
#define GRAPH_X (Out.width - GRAPH_LINE_WIDTH - 2)
#define GRAPH_Y 283
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
static uint16_t blestateicon;
static uint16_t lastblestateicon;
static uint16_t dbIconColor;
static uint16_t lastdbIconColor;
static unsigned long blinknow = millis(); // task blinker start time
static unsigned long scanTime = SCAN_DURATION * 1000; // task blinker duration
static unsigned long blinkthen = blinknow + scanTime; // task blinker end time
static unsigned long lastblink = millis(); // task blinker last blink
static unsigned long lastprogress = millis(); // task blinker progress

const char* sessDevicesCountTpl = " Seen: %4d ";
const char* devicesCountTpl = " Last: %4d ";
const char* newDevicesCountTpl = " Scans:%4d ";
const char* heapTpl = " Heap: %6d";
const char* entriesTpl = " Entries:%4d";
const char *addressTpl = "  %s";
const char* dbmTpl = "%d dBm    ";
const char* ouiTpl = "      %s";
const char* appearanceTpl = "  Appearance: %d";
const char* manufTpl = "      %s";
const char* nameTpl = "      %s";

//#define SPACETABS "      "


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


char *macAddressToColorStr = (char*)calloc(MAC_LEN+1, sizeof(char*));
byte macBytesToBMP[8];
uint16_t macBytesToColors[128];

uint16_t macAddressToColor( const char *address ) {
  memcpy( macAddressToColorStr, address, MAC_LEN+1);
  char *token;
  char *ptr;
  byte tokenpos = 0, msb, lsb;
  token = strtok(macAddressToColorStr, ":");
  while(token != NULL) {
    byte val = strtol(token, &ptr, 16);
    switch( tokenpos )  {
      case 0: msb = val; break;
      case 1: lsb = val; break;
      default: 
        byte macpos = tokenpos-2;
        macBytesToBMP[macpos] = val; 
        macBytesToBMP[7-macpos]= val;
      break;
    }
    tokenpos++;
    token = strtok(NULL, ":");
  }
  uint16_t color = (msb*256) + lsb;
  byte curs = 0;
  for(byte i=0;i<8;i++) {
    for(byte j=0;j<8;j++) {
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

static uint16_t imgBuffer[320];

//#define SCREEN_CAP_BUFSIZE GRAPH_LINE_WIDTH*GRAPH_LINE_HEIGHT

uint16_t screenCapBuffer[76*40];

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
      tft.setRotation( 0 ); // required to get smooth scrolling
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
        headerStats("Heap heap heap...");
      } else {
        headerStats("Init UI");
      }
      Out.setupScrollArea(HEADER_HEIGHT, FOOTER_HEIGHT);
      tft.setTextColor( BLE_WHITE, BLECARD_BGCOLOR );

      SDSetup();
      timeSetup();
      #if RTC_PROFILE > ROGUE // NTP_MENU and CHRONOMANIAC SD-mirror themselves
        selfReplicateToSD();
      #endif
      timeStateIcon();
      footerStats();
      xTaskCreatePinnedToCore(taskHeapGraph, "taskHeapGraph", 2048, NULL, 2, NULL, 1);
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
      drawRoundRect( 56, Out.scrollPosY, 128, pos, 8, BLE_GREENYELLOW );
      //drawRoundRect( 1,  Out.scrollPosY-blockHeight, Out.width - 2,  blockHeight -2, 4, BLECardTheme.borderColor );
      for (int i = 0; i < 5; i++) {
        Out.println(SPACE);
      }
      giveMuxSemaphore();
      xTaskCreatePinnedToCore(introUntilScroll, "introUntilScroll", 2048, NULL, 1, NULL, 1);
    }


    static void screenShot() {
      const char* screenshotFilenameTpl = "/screenshot-%04d-%02d-%02d_%02dh%02dm%02ds.565";
      char fileName[42];
      sprintf(fileName, screenshotFilenameTpl, year(), month(), day(), hour(), minute(), second());
      //Serial.println("Will open file for creation");
      File screenshotFile = BLE_FS.open( fileName, FILE_WRITE);
      if(!screenshotFile) {
        screenshotFile.close();
        return;
      }
      uint16_t screenshotWidth  = Out.width;
      uint16_t screenshotHeight = Out.height;
      //Serial.printf("Will scan %d lines of %d pixels / %d bytes\n", screenshotHeight, screenshotWidth, sizeof(uint16_t)*screenshotWidth);
      // TODO: compensate for the scrollArea jump
      for(uint16_t y=0; y<screenshotHeight; y++) {
        memset((uint8_t*)imgBuffer, 0, sizeof(uint16_t)*screenshotWidth);
        takeMuxSemaphore();
        tft.readPixels(0, y, screenshotWidth, 1, imgBuffer);
        screenshotFile.write( (uint8_t*)imgBuffer, sizeof(uint16_t)*screenshotWidth );
        giveMuxSemaphore();
        vTaskDelay(1);
      }
      screenshotFile.close();
      Serial.printf("Screenshot saved as %s, now go to http://rawpixels.net/ to decode it (RGB565 240x320 Little Endian)\n", fileName);
    }

    static void screenShow( void * param = NULL ) {
      if( param == NULL ) return;
      Serial.printf("Should open file %s\n", (const char*) param);
      File screenshotFile = BLE_FS.open( (const char*)param );
      if(!screenshotFile) {
        screenshotFile.close();
        return;
      }
      uint16_t screenshotWidth  = Out.width;
      uint16_t screenshotHeight = Out.height;
      for(uint16_t y=0; y<screenshotHeight; y++) {
        memset((uint8_t*)imgBuffer, 0, sizeof(uint16_t)*screenshotWidth);
        takeMuxSemaphore();
        screenshotFile.read( (uint8_t*)imgBuffer, sizeof(uint16_t)*screenshotWidth );
        tft.startWrite();
        tft.setAddrWindow(0, y, screenshotWidth, 1);
        tft.writePixels(imgBuffer, screenshotWidth);
        tft.endWrite();
        giveMuxSemaphore();
      }
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


    void headerStats(const char *status = "") {
      if (isScrolling) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();
      int16_t statuspos = 0;

      char heapStr[16] = {'\0'};
      sprintf(heapStr, heapTpl, freeheap);

      char entriesStr[14] = {'\0'};
      sprintf(entriesStr, entriesTpl, entries);
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

      alignTextAt( hhmmString,   85, 288, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt( UpTimeString, 85, 298, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt("(c+) tobozo", 77, 308, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      char sessDevicesCountStr[16] = {'\0'};
      char devicesCountStr[16] = {'\0'};
      char newDevicesCountStr[16] = {'\0'};

      sprintf( sessDevicesCountStr, sessDevicesCountTpl, sessDevicesCount );
      sprintf( devicesCountStr, devicesCountTpl, devicesCount );
      sprintf( newDevicesCountStr, newDevicesCountTpl, scan_rounds );

      alignTextAt( devicesCountStr, 0, 288, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );
      alignTextAt( sessDevicesCountStr, 0, 298, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );
      alignTextAt( newDevicesCountStr, 0, 308, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_LEFT );

      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void cacheStats() {
      takeMuxSemaphore();
      percentBox(164, 284, 10, 10, BLEDevCacheUsed, BLE_CYAN, BLE_BLACK);
      percentBox(164, 296, 10, 10, VendorCacheUsed, BLE_ORANGE, BLE_BLACK);
      percentBox(164, 308, 10, 10, OuiCacheUsed, BLE_GREENYELLOW, BLE_BLACK);
      if( filterVendors ) {
        tft.drawJpg( filter_jpeg, filter_jpeg_len, 152, 308, 10,  8);
      } else {
        tft.fillRect( 152, 308, 10,  8, FOOTER_BGCOLOR );
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
      if (RTC_is_running) {
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
      xTaskCreatePinnedToCore(clockSync, "clockSync", 2048, NULL, 4, NULL, 1); // RTC wants to run on core 1 or it fails
      vTaskDelete(NULL);
    }


    static void clockSync(void * parameter) {
      unsigned long lastClockTick = millis();
      while(1) {
        if(lastClockTick + 1000 > millis()) {
          vTaskDelay( 100 );
          continue;
        }
        takeMuxSemaphore();
        timeHousekeeping();
        giveMuxSemaphore();
        lastClockTick = millis();
        vTaskDelay( 100 );
      }
    }


    static void heapGraph(void * parameter) {
      uint32_t lastfreeheap;
      uint32_t toleranceheap = min_free_heap + heap_tolerance;
      uint8_t i = 0;

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
          log_e("Filtering %s with random vendorname", BleCard->address);
          return;
        }
      }

      log_d("  [printBLECard] %s will be rendered", BleCard->address);

      takeMuxSemaphore();
      memcpy( lastPrintedMac[lastPrintedMacIndex++ % BLECARD_MAC_CACHE_SIZE], BleCard->address, MAC_LEN+1 );
      uint16_t randomcolor = tft.color565( random(128, 255), random(128, 255), random(128, 255) );
      uint16_t blockHeight = 0;
      uint16_t hop;

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


      char addressStr[24] = {'\0'};
      sprintf( addressStr, addressTpl, BleCard->address );
      hop = Out.println( addressStr );
      blockHeight += hop;

      //if ( !isEmpty( BleCard->ouiname ) && strcmp( BleCard->ouiname, "[random]" )!=0 ) {
        uint16_t macColor = macAddressToColor( (const char*) BleCard->address );
        tft.drawBitmap( 150, Out.scrollPosY - hop, 16, 8, macBytesToColors );
        //maclastbytes
        //tft.setTextColor( macColor, BLE_WHITE /*BLECardTheme.bgColor */);
      //}
      
      char dbmStr[16];
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

      if( Time_is_set ) {
        if ( BleCard->hits > 1 ) {
  
          char hitsTimeStampStr[48];
          const char* hitsTimeStampTpl = "      %04d/%02d/%02d %02d:%02d:%02d %s";
          char hitsStr[16];
  
          if( BleCard->hits > 999 ) {
            sprintf(hitsStr, "(%dK hits)", BleCard->hits/1000);
          } else {
            sprintf(hitsStr, "(%d hits)", BleCard->hits);
          }
  
          if( BleCard->updated_at.unixtime() > 0 /* BleCard->created_at.year() > 1970 */) {
            unsigned long age_in_seconds = abs( BleCard->created_at.unixtime() - BleCard->updated_at.unixtime() );
            unsigned long age_in_minutes = age_in_seconds / 60;
            unsigned long age_in_hours   = age_in_minutes / 60;
            unsigned long seconds_since_boot = (millis() / 1000)+1;
            float freq = ((float)BleCard->hits / (float)scan_rounds+1) * ((float)age_in_seconds / (float)seconds_since_boot+1);
            Serial.printf(" C: %d, U:%d, (%d / %d) * (%d / %d) = ",
              BleCard->created_at.unixtime(),
              BleCard->updated_at.unixtime(),
              BleCard->hits,
              scan_rounds+1,
              age_in_seconds+1,
              millis() / 1000
            );
            Serial.println( freq * 1000 );
          }

          if( BleCard->created_at.year() > 1970 ) {
            blockHeight += Out.println(SPACE);
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
        char ouiStr[38] = {'\0'};
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
        char appearanceStr[48];
        sprintf( appearanceStr, appearanceTpl, BleCard->appearance );
        hop = Out.println( appearanceStr );
        blockHeight += hop;
      }
      if ( !isEmpty( BleCard->manufname ) ) {
        blockHeight += Out.println(SPACE);
        char manufStr[38] = {'\0'};
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
        char nameStr[38] = {'\0'};
        sprintf(nameStr, nameTpl, BleCard->name);
        blockHeight += Out.println(SPACE);
        hop = Out.println( nameStr );
        blockHeight += hop;
        tft.drawJpg( name_jpeg, name_jpeg_len, 12, Out.scrollPosY - hop, 7,  8);
      }
      hop = Out.println( SPACE) ;
      blockHeight += hop;
      drawRoundRect( 0, Out.scrollPosY, Out.width - 2, blockHeight, 4, BLECardTheme.borderColor );
      giveMuxSemaphore();
    }


    static bool BLECardIsOnScreen( const char* address ) {
      log_d("Checking if %s is onScreen", address);
      bool onScreen = false;
      for (uint16_t j = 0; j < BLECARD_MAC_CACHE_SIZE; j++) {
        if ( strcmp( address, lastPrintedMac[j] ) == 0) {
          log_d("%s is onScreen", address);
          onScreen = true;
          return true;
        }
        delay(1);
      }
      log_d("%s is NOT onScreen", address);
      return onScreen;      
    }


    static bool BLECardIsOnScreen( BlueToothDevice *CacheItem, uint16_t CacheItemIndex ) {
      if( isEmpty( CacheItem[CacheItemIndex].address ) ) {
        log_w("Empty address !!");
        return false;
      }
      return BLECardIsOnScreen( CacheItem[CacheItemIndex].address );
    }


  private:

    void alignTextAt(const char* text, uint16_t x, uint16_t y, int16_t color = BLE_YELLOW, int16_t bgcolor = BGCOLOR, byte textAlign = ALIGN_FREE) {
      tft.setTextColor(color, bgcolor);
      tft.getTextBounds(text, x, y, &Out.x1_tmp, &Out.y1_tmp, &Out.w_tmp, &Out.h_tmp);
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

    // draw rounded corners boxes over the scroll limit
    void drawRoundRect(uint16_t x, int y, uint16_t width, uint16_t height, uint16_t radius, uint16_t color) {
      int scrollPosY = y/*Out.scrollPosY*/ - height;
      if (scrollPosY >= Out.scrollTopFixedArea) {
        // no scroll loop point overlap, just render the box
        tft.drawRoundRect(x+1, scrollPosY + 1, width, height - 2, radius, color);
      } else {
        // last block overlaps scroll loop point and has been split
        int h1 = (Out.scrollTopFixedArea - scrollPosY);
        int h2 = height - h1;
        h1 -= 2; //margin
        h2 -= 2;; //margin
        int vpos1 = scrollPosY + Out.yArea + 1;
        int vpos2 = y/*Out.scrollPosY*/ - 2;
        tft.drawFastHLine(x+1 + radius, vpos1, width - 2 * radius, color); // upper hline
        tft.drawFastHLine(x+1 + radius, vpos2, width - 2 * radius, color); // lower hline
        if (h1 > radius) {
          tft.drawFastVLine(x+1,      vpos1 + radius, h1 - radius + 1, color); // upper left vline
          tft.drawFastVLine(x+width,  vpos1 + radius, h1 - radius + 1, color); // upper right vline
        }
        if (h2 > radius) {
          tft.drawFastVLine(x+1,      vpos2 - h2, h2 - radius + 1, color); // lower left vline
          tft.drawFastVLine(x+width,  vpos2 - h2, h2 - radius + 1, color); // lower right vline
        }
        tft.startWrite();//BLEDev.borderColor
        tft.drawCircleHelper(x+1 + radius,      vpos1 + radius, radius, 1, color); // upper left
        tft.drawCircleHelper(x+width - radius,  vpos1 + radius, radius, 2, color); // upper right
        tft.drawCircleHelper(x+width - radius,  vpos2 - radius, radius, 4, color); // lower right
        tft.drawCircleHelper(x+1 + radius,      vpos2 - radius, radius, 8, color); // lower left
        tft.endWrite();
      }
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
