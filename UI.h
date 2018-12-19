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
#define SCROLL_HEIGHT ( tft.height() - ( HEADER_HEIGHT + FOOTER_HEIGHT ))

// heap map settings
#define HEAPMAP_BUFFLEN 61 // graph width (+ 1 for hscroll)
#define MAX_ROW_LEN 30 // max chars per line on display, used to position/cut text
// heap management (used by graph)
static uint32_t min_free_heap = 90000; // sql out of memory errors eventually occur under 100000
static uint32_t initial_free_heap = freeheap;
static uint32_t heap_tolerance = 20000; // how much memory under min_free_heap the sketch can go and recover without restarting itself
static uint32_t heapmap[HEAPMAP_BUFFLEN] = {0}; // stores the history of heapmap values
static uint16_t heapindex = 0; // index in the circular buffer
static bool blinkit = false; // task blinker state
static bool blinktoggler = true;
static bool appIconVisible = false;
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




class UIUtils {
  public:

    struct BLECardStyle {
      uint16_t textColor = BLE_WHITE;
      uint16_t borderColor = BLE_WHITE;
      uint16_t bgColor = BLECARD_BGCOLOR;
      void setTheme( byte themeID ) {
        bgColor = BLECARD_BGCOLOR;
        switch ( themeID ) {
          case IN_CACHE_ANON:// = 0,
            borderColor = IN_CACHE_COLOR;
            textColor = ANONYMOUS_COLOR;
            break;
          case IN_CACHE_NOT_ANON:// = 1,
            borderColor = IN_CACHE_COLOR;
            textColor = NOT_ANONYMOUS_COLOR;
            break;
          case NOT_IN_CACHE_ANON:// = 2,
            borderColor = NOT_IN_CACHE_COLOR;
            textColor = ANONYMOUS_COLOR;
            break;
          case NOT_IN_CACHE_NOT_ANON:// = 3
            borderColor = NOT_IN_CACHE_COLOR;
            textColor = NOT_ANONYMOUS_COLOR;
            break;
        }
      }
    };

    BLECardStyle BLECardTheme;

    void init() {

      bool clearScreen = true;
      if (resetReason == 12) { // SW Reset
        clearScreen = false;
      }
      //GO.begin();
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
      timeSetup();
      updateTimeString( true );
      timeStateIcon();
      footerStats();

      #if RTC_PROFILE!=NTP_MENU
        xTaskCreatePinnedToCore(taskHeapGraph, "taskHeapGraph", 2048, NULL, 2, NULL, 1);
      #endif

      if ( clearScreen ) {
        playIntro();
      }
    }


    void update() {
      if ( freeheap + heap_tolerance < min_free_heap ) {
        headerStats("Out of heap..!");
        Serial.printf("[FATAL] Heap too low: %d\n", freeheap);
        delay(1000);
        ESP.restart();
      }
      headerStats();
      footerStats();
    }


    void playIntro() {
      uint16_t pos = 0;
      tft.setTextColor(BLE_GREENYELLOW, BGCOLOR);
      for (int i = 0; i < 5; i++) {
        pos += Out.println();
      }
      pos += Out.println("         /---------------------\\");
      pos += Out.println("         | ESP32 BLE Collector |");
      pos += Out.println("         | ------------------- |");
      pos += Out.println("         | (c+)  tobozo  2018  |");
      pos += Out.println("         \\---------------------/");
      tft.drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, 106, Out.scrollPosY - pos + 8, 28,  28);
      for (int i = 0; i < 5; i++) {
        Out.println(SPACE);
      }
      AmigaBall.animate( 5000 );
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

      alignTextAt( hhmmString,   95, 288, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt( UpTimeString, 95, 298, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt("(c+) tobozo", 77, 308, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      char sessDevicesCountStr[16] = {'\0'};
      char devicesCountStr[16] = {'\0'};
      char newDevicesCountStr[16] = {'\0'};

      sprintf( sessDevicesCountStr, sessDevicesCountTpl, sessDevicesCount );
      sprintf( devicesCountStr, devicesCountTpl, devicesCount );
      sprintf( newDevicesCountStr, newDevicesCountTpl, scan_rounds/*newDevicesCount*/ );

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
        //Serial.printf("[%s] blinked at %d\n", __func__, dbIconColor);
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
        } else {
          tft.fillCircle(ICON_BLE_X, ICON_BLE_Y, ICON_R - 1, blestateicon);
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
        //takeMuxSemaphore();
        updateTimeString();
        //giveMuxSemaphore();
        lastClockTick = millis();
        vTaskDelay( 100 );
      }
    }


    static void heapGraph(void * parameter) {
      uint32_t lastfreeheap;
      uint32_t toleranceheap = min_free_heap + heap_tolerance;
      uint8_t i = 0;
      uint32_t GRAPH_LINE_WIDTH = HEAPMAP_BUFFLEN - 1;
      uint32_t GRAPH_LINE_HEIGHT = 35;
      uint16_t GRAPH_X = Out.width - GRAPH_LINE_WIDTH - 2;
      uint16_t GRAPH_Y = 283;
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


    void printBLECard( BlueToothDevice *_BLEDevTmp ) {
      // don't render if already on screen
      if( BLECardIsOnScreen( _BLEDevTmp->address ) ) {
        Serial.printf("  [printBLECard] %s is already on screen, skipping rendering\n", _BLEDevTmp->address);
        return;
      }

      if ( isEmpty( _BLEDevTmp->address ) ) {
        Serial.printf("  [printBLECard] Cowardly refusing to render %d with an empty address\n", 0);
        return;        
      }

      Serial.printf("  [printBLECard] %s will be rendered\n", _BLEDevTmp->address);

      takeMuxSemaphore();

      uint16_t randomcolor = tft.color565( random(128, 255), random(128, 255), random(128, 255) );
      uint16_t pos = 0;
      uint16_t hop;

      if( _BLEDevTmp->is_anonymous ) {
        BLECardTheme.setTheme( IN_CACHE_ANON );
      } else {
        BLECardTheme.setTheme( IN_CACHE_NOT_ANON );
      }
      
      tft.setTextColor( BLECardTheme.textColor, BLECardTheme.bgColor );
      BGCOLOR = BLECardTheme.bgColor;
      hop = Out.println( SPACE );
      pos += hop;

      memcpy( lastPrintedMac[lastPrintedMacIndex++ % BLECARD_MAC_CACHE_SIZE], _BLEDevTmp->address, MAC_LEN+1 );

      char addressStr[24] = {'\0'};
      sprintf( addressStr, addressTpl, _BLEDevTmp->address );
      hop = Out.println( addressStr );
      pos += hop;
      char dbmStr[16];

      sprintf( dbmStr, dbmTpl, _BLEDevTmp->rssi );
      alignTextAt( dbmStr, 0, Out.scrollPosY - hop, BLECardTheme.textColor, BLECardTheme.bgColor, ALIGN_RIGHT );
      tft.setCursor( 0, Out.scrollPosY );
      drawRSSI( Out.width - 18, Out.scrollPosY - hop - 1, _BLEDevTmp->rssi, BLECardTheme.textColor );
      if ( _BLEDevTmp->in_db ) { // 'already seen this' icon
        tft.drawJpg( update_jpeg, update_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      } else { // 'just inserted this' icon
        tft.drawJpg( insert_jpeg, insert_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      }
      if ( !isEmpty( _BLEDevTmp->uuid ) ) { // 'has service UUID' Icon
        tft.drawJpg( service_jpeg, service_jpeg_len, 128, Out.scrollPosY - hop, 8,  8);
      }

      switch( _BLEDevTmp->hits ) {
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

      #if RTC_PROFILE > HOBO 
      if ( _BLEDevTmp->hits > 1 ) {
        pos += Out.println(SPACE);

        char timeStampStr[48];
        const char* timeStampTpl = "      %s: %04d/%02d/%02d %02d:%02d:%02d %s%s%s";
        sprintf(timeStampStr, timeStampTpl, 
          "C",
          _BLEDevTmp->created_at.year(),
          _BLEDevTmp->created_at.month(),
          _BLEDevTmp->created_at.day(),
          _BLEDevTmp->created_at.hour(),
          _BLEDevTmp->created_at.minute(),
          _BLEDevTmp->created_at.second(),
          "(",
          String(_BLEDevTmp->hits).c_str(),
          " hits)"
        );
        hop = Out.println( timeStampStr );
        pos += hop;

        tft.drawJpg( clock_jpeg, clock_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );

/*
        pos += Out.println(SPACE);
        sprintf(timeStampStr, timeStampTpl, 
          "U",
          _BLEDevTmp->updated_at.year(),
          _BLEDevTmp->updated_at.month(),
          _BLEDevTmp->updated_at.day(),
          _BLEDevTmp->updated_at.hour(),
          _BLEDevTmp->updated_at.minute(),
          _BLEDevTmp->updated_at.second(),
          "", "", ""
        );
        hop = Out.println( timeStampStr );
        pos += hop;

        tft.drawJpg( clock_jpeg, clock_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
 */       
        
      }
      #endif

      if ( !isEmpty( _BLEDevTmp->ouiname ) ) {
        pos += Out.println( SPACE );
        char ouiStr[38] = {'\0'};
        sprintf( ouiStr, ouiTpl, _BLEDevTmp->ouiname );
        hop = Out.println( ouiStr );
        pos += hop;
        tft.drawJpg( nic16_jpeg, nic16_jpeg_len, 10, Out.scrollPosY - hop, 13, 8 );
      }
      
      if ( _BLEDevTmp->appearance != 0 ) {
        pos += Out.println(SPACE);
        char appearanceStr[48];
        sprintf( appearanceStr, appearanceTpl, _BLEDevTmp->appearance );
        hop = Out.println( appearanceStr );
        pos += hop;
      }
      if ( !isEmpty( _BLEDevTmp->manufname ) ) {
        pos += Out.println(SPACE);
        char manufStr[38] = {'\0'};
        sprintf( manufStr, manufTpl, _BLEDevTmp->manufname );
        hop = Out.println( manufStr );
        pos += hop;
        if ( strstr( _BLEDevTmp->manufname, "Apple" ) ) {
          tft.drawJpg( apple16_jpeg, apple16_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( _BLEDevTmp->manufname, "IBM" ) ) {
          tft.drawJpg( ibm8_jpg, ibm8_jpg_len, 10, Out.scrollPosY - hop, 20,  8);
        } else if ( strstr (_BLEDevTmp->manufname, "Microsoft" ) ) {
          tft.drawJpg( crosoft_jpeg, crosoft_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( _BLEDevTmp->manufname, "Bose" ) ) {
          tft.drawJpg( speaker_icon_jpg, speaker_icon_jpg_len, 12, Out.scrollPosY - hop, 6,  8 );
        } else {
          tft.drawJpg( generic_jpeg, generic_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        }
      }
      if ( !isEmpty( _BLEDevTmp->name ) ) {
        char nameStr[38] = {'\0'};
        sprintf(nameStr, nameTpl, _BLEDevTmp->name);
        pos += Out.println(SPACE);
        hop = Out.println( nameStr );
        pos += hop;
        tft.drawJpg( name_jpeg, name_jpeg_len, 12, Out.scrollPosY - hop, 7,  8);
      }
      hop = Out.println( SPACE) ;
      pos += hop;
      drawRoundRect( pos, 4, BLECardTheme.borderColor );
      giveMuxSemaphore();
    }


    static bool BLECardIsOnScreen( const char* address ) {
      //Serial.printf("[BLECardIsOnScreen] Checking if %s is onScreen\n", CacheItem[CacheItemIndex].address);
      bool onScreen = false;
      for (uint16_t j = 0; j < BLECARD_MAC_CACHE_SIZE; j++) {
        if ( strcmp( address, lastPrintedMac[j] ) == 0) {
          //Serial.printf("  [BLECardIsOnScreen] %s is onScreen\n", address);
          onScreen = true;
          return true;
        }
        delay(1);
      }
      //Serial.printf("  [BLECardIsOnScreen] %s is NOT onScreen\n", address);
      return onScreen;      
    }


    static bool BLECardIsOnScreen( BlueToothDevice *CacheItem, uint16_t CacheItemIndex ) {
      if( isEmpty( CacheItem[CacheItemIndex].address ) ) {
        Serial.print("[BLECardIsOnScreen] Empty address !!");
        return false;
      }
      return BLECardIsOnScreen( CacheItem[CacheItemIndex].address );
    }


  private:

    void alignTextAt(const char* text, uint16_t x, uint16_t y, int16_t color = BLE_YELLOW, int16_t bgcolor = BLE_BLACK, byte textAlign = ALIGN_FREE) {
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
    void drawRoundRect(uint16_t pos, uint16_t radius, uint16_t color) {
      if (Out.scrollPosY - pos >= Out.scrollTopFixedArea) {
        // no scroll loop point overlap, just render the box
        tft.drawRoundRect(1, Out.scrollPosY - pos + 1, Out.width - 2, pos - 2, 4, color);
      } else {
        // last block overlaps scroll loop point and has been split
        int h1 = (Out.scrollTopFixedArea - (Out.scrollPosY - pos));
        int h2 = pos - h1;
        int lwidth = Out.width - 2;
        h1 -= 2; //margin
        h2 -= 2;; //margin
        int vpos1 = Out.scrollPosY - pos + Out.yArea + 1;
        int vpos2 = Out.scrollPosY - 2;
        tft.drawFastHLine(1 + radius, vpos1, lwidth - 2 * radius, color); // upper hline
        tft.drawFastHLine(1 + radius, vpos2, lwidth - 2 * radius, color); // lower hline
        if (h1 > radius) {
          tft.drawFastVLine(1,      vpos1 + radius, h1 - radius + 1, color); // upper left vline
          tft.drawFastVLine(lwidth, vpos1 + radius, h1 - radius + 1, color); // upper right vline
        }
        if (h2 > radius) {
          tft.drawFastVLine(1,      vpos2 - h2, h2 - radius + 1, color); // lower left vline
          tft.drawFastVLine(lwidth, vpos2 - h2, h2 - radius + 1, color); // lower right vline
        }
        tft.startWrite();//BLEDev.borderColor
        tft.drawCircleHelper(1 + radius,      vpos1 + radius, radius, 1, color); // upper left
        tft.drawCircleHelper(lwidth - radius, vpos1 + radius, radius, 2, color); // upper right
        tft.drawCircleHelper(lwidth - radius, vpos2 - radius, radius, 4, color); // lower right
        tft.drawCircleHelper(1 + radius,      vpos2 - radius, radius, 8, color); // lower left
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
