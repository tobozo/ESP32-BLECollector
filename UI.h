
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


// heap map settings
#define HEAPMAP_BUFFLEN 61 // graph width (+ 1 for hscroll)

// variables used to manage landscape/portrait displays
int16_t GRAPH_LINE_WIDTH;
int16_t GRAPH_LINE_HEIGHT;
int16_t GRAPH_X;
int16_t GRAPH_Y;
int16_t PERCENTBOX_X;
int16_t PERCENTBOX_Y;
int16_t HEADERSTATS_X;
int16_t FOOTER_BOTTOMPOS;
int16_t PERCENTBOX_SIZE;
int16_t HEADERSTATS_ICONS_X;
int16_t HEADERSTATS_ICONS_Y;
int16_t PROGRESSBAR_Y;
int16_t HEADER_LINEHEIGHT;
int16_t HHMM_POSX;
int16_t HHMM_POSY;
int16_t UPTIME_POSX;
int16_t UPTIME_POSY;
int16_t COPYLEFT_POSX;
int16_t COPYLEFT_POSY;
int16_t CDEV_C_POSX;
int16_t CDEV_C_POSY;
int16_t SESS_C_POSX;
int16_t SESS_C_POSY;
int16_t NDEV_C_POSX;
int16_t NDEV_C_POSY;
int16_t GPSICON_POSX;
int16_t GPSICON_POSY;
int16_t HEADER_HEIGHT;
int16_t FOOTER_HEIGHT;
int16_t SCROLL_HEIGHT;
// icon positions for RTC/DB/BLE
int16_t ICON_APP_X = 124;
int16_t ICON_APP_Y = 0;
int16_t ICON_RTC_X = 92;
int16_t ICON_RTC_Y = 7;
int16_t ICON_BLE_X = 104;
int16_t ICON_BLE_Y = 7;
int16_t ICON_DB_X = 116;
int16_t ICON_DB_Y = 7;
int16_t ICON_R = 4;


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
static bool fileSharingEnabled = false;
static bool foundFileServer = false;
static bool gpsIconVisible = false;
static bool uptimeIconWasRendered = false;
static bool foundFileServerIconWasRendered = true;
static uint16_t blestateicon;
static uint16_t lastblestateicon;
static uint16_t dbIconColor;
static uint16_t lastdbIconColor;
static unsigned long blinknow = millis(); // task blinker start time
static unsigned long scanTime = SCAN_DURATION * 1000; // task blinker duration
static unsigned long blinkthen = blinknow + scanTime; // task blinker end time
static unsigned long lastblink = millis(); // task blinker last blink
static unsigned long lastprogress = millis(); // task blinker progress

const char* sessDevicesCountTpl = "Seen: %4s";
const char* devicesCountTpl = "Last: %4s";
const char* newDevicesCountTpl = "Scans:%4s";
const char* heapTpl = "Heap: %6d";
const char* entriesTpl = "Entries:%4s";
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
      Serial.begin(115200);
      Serial.println(welcomeMessage);
      Serial.printf("RTC_PROFILE: %s\nHAS_EXTERNAL_RTC: %s\nHAS_GPS: %s\nTIME_UPDATE_SOURCE: %d\nSKECTH_MODE: %d\n",
        RTC_PROFILE,
        HAS_EXTERNAL_RTC ? "true" : "false",
        HAS_GPS ? "true" : "false",
        TIME_UPDATE_SOURCE,
        SKETCH_MODE
      );
      Serial.println("Free heap at boot: " + String(initial_free_heap));

      bool clearScreen = true;
      if (resetReason == 12) { // SW Reset
        clearScreen = false;
      }

      tft_begin();
      tft_initOrientation(); //tft.setRotation( 0 ); // required to get smooth scrolling
      setUISizePos(); // set position/dimensions for widgets and other UI items
      tft.setTextColor(BLE_YELLOW);

      if (clearScreen) {
        tft.fillScreen(BLE_BLACK);
        tft.fillRect(0, HEADER_HEIGHT, Out.width, SCROLL_HEIGHT, BLECARD_BGCOLOR);
        // clear heap map
        for (uint16_t i = 0; i < HEAPMAP_BUFFLEN; i++) heapmap[i] = 0;
      }

      tft.fillRect(0, 0, Out.width, HEADER_HEIGHT, HEADER_BGCOLOR);
      tft.fillRect(0, FOOTER_BOTTOMPOS - FOOTER_HEIGHT, Out.width, FOOTER_HEIGHT, FOOTER_BGCOLOR);
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
      if ( clearScreen ) {
        playIntro();
      } else {
        Out.scrollNextPage();
      }
      
    }

    void begin() {
      xTaskCreatePinnedToCore(taskHeapGraph, "taskHeapGraph", 1024, NULL, 0, NULL, 1);
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

      pos += Out.println(SPACE);
      alignTextAt( "ESP32 BLE Collector", 6, Out.scrollPosY, BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_CENTER );
      pos += Out.println(SPACE);
      pos += Out.println(SPACE);
      alignTextAt( "(c+)  tobozo  2019", 6, Out.scrollPosY, BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_CENTER );
      pos += Out.println(SPACE);
      pos += Out.println(SPACE);
      tft_drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, (Out.width/2 - 14), Out.scrollPosY - pos + 8, 28,  28);
      Out.drawScrollableRoundRect( (Out.width/2 - 64), Out.scrollPosY-pos, 128, pos, 8, BLE_GREENYELLOW );

      /*
      pos += Out.println("         ");
      pos += Out.println("           ESP32 BLE Collector  ");
      pos += Out.println("         ");
      pos += Out.println("           (c+)  tobozo  2018   ");
      pos += Out.println("         ");
      tft_drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, 106, Out.scrollPosY - pos + 8, 28,  28);
      Out.drawScrollableRoundRect( 58, Out.scrollPosY-pos, 128, pos, 8, BLE_GREENYELLOW );
      */
      for (int i = 0; i < 5; i++) {
        Out.println(SPACE);
      }
      giveMuxSemaphore();
      delay(2000);
      takeMuxSemaphore();
      Out.scrollNextPage();
      giveMuxSemaphore();
      #ifndef SKIP_INTRO
      xTaskCreatePinnedToCore(introUntilScroll, "introUntilScroll", 2048, NULL, 1, NULL, 1);
      #endif
    }


    static void screenShot() {

      takeMuxSemaphore();
      isQuerying = true;
      M5.ScreenShot.snap("BLECollector", true);
      isQuerying = false;
      giveMuxSemaphore();

    }

    static void screenShow( void * fileName = NULL ) {
      if( fileName == NULL ) return;
      isQuerying = true;

      if( String( (const char*)fileName ).endsWith(".jpg" ) ) {
        if( !BLE_FS.exists( (const char*)fileName ) ) {
          log_e("File %s does not exist\n", (const char*)fileName );
          isQuerying = false;
          return;
        }
        takeMuxSemaphore();
        Out.scrollNextPage(); // reset scroll position to zero otherwise image will have offset
        tft.drawJpgFile( BLE_FS, (const char*)fileName, 0, 0, Out.width, Out.height, 0, 0, JPEG_DIV_NONE );
        giveMuxSemaphore();
        vTaskDelay( 5000 );
        return;
      }
      if( String( (const char*)fileName ).endsWith(".565" ) ) {
        File screenshotFile = BLE_FS.open( (const char*)fileName );
        if(!screenshotFile) {
          Serial.printf("Failed to open file %s\n", (const char*)fileName );
          screenshotFile.close();
          return;
        }
        takeMuxSemaphore();
        Out.scrollNextPage(); // reset scroll position to zero otherwise image will have offset
        for(uint16_t y=0; y<Out.height; y++) {
          screenshotFile.read( (uint8_t*)imgBuffer, sizeof(uint16_t)*Out.width );
          tft_drawBitmap(0, y, Out.width, 1, imgBuffer);
        }
        giveMuxSemaphore();
      }
      isQuerying = false;
    }


    static void introUntilScroll( void * param ) {
      int initialscrollPosY = Out.scrollPosY;
      // animate until the scroll is called
      while(initialscrollPosY == Out.scrollPosY) {
        takeMuxSemaphore();
        AmigaBall.animate(1, false);
        giveMuxSemaphore();
        delay(1);
      }
      vTaskDelete(NULL);
    }


    static void headerStats(const char *status = "") {
      if ( isInScroll() || isInQuery() ) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();
      int16_t statuspos = 0;
      *heapStr = {'\0'};
      *entriesStr = {'\0'};
      if ( !isEmpty( status ) ) {
        byte alignoffset = 5;
        tft.fillRect(0, HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, ICON_APP_X, 8, HEADER_BGCOLOR); // clear whole message status area
        if (strstr(status, "Inserted")) {
          tft_drawJpg(disk_jpeg, disk_jpeg_len, alignoffset,   HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, 8, 8); // disk icon
          alignoffset +=10;
        } else if (strstr(status, "Cache")) {
          tft_drawJpg(ghost_jpeg, ghost_jpeg_len, alignoffset, HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, 8, 8); // disk icon
          alignoffset +=10;
        } else if (strstr(status, "DB")) {
          tft_drawJpg(moai_jpeg, moai_jpeg_len, alignoffset,   HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, 8, 8); // disk icon
          alignoffset +=10;
        } else {
          
        }
        alignTextAt( status, alignoffset, HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, BLE_YELLOW, HEADER_BGCOLOR, ALIGN_FREE );
        statuspos = Out.x1_tmp + Out.w_tmp;
      }

      sprintf(heapStr, heapTpl, freeheap);
      sprintf(entriesStr, entriesTpl, formatUnit(entries));
      alignTextAt( heapStr,    HEADERSTATS_X, HEADERSTATS_ICONS_Y,      BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_RIGHT );
      alignTextAt( entriesStr, HEADERSTATS_X, HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, BLE_GREENYELLOW, HEADER_BGCOLOR, ALIGN_RIGHT );

      if( !appIconVisible || statuspos > ICON_APP_X ) { // only draw if text has overlapped
        tft_drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, ICON_APP_X, ICON_APP_Y, 28,  28); // app icon
        appIconVisible = true;
      }

      tft_drawJpg(earth_jpeg, earth_jpeg_len, HEADERSTATS_ICONS_X, HEADERSTATS_ICONS_Y + HEADER_LINEHEIGHT, 8, 8); // entries icon

      if( foundFileServer ) {
        if( !foundFileServerIconWasRendered ) {
          tft.fillRect( HEADERSTATS_ICONS_X, HEADERSTATS_ICONS_Y, 8, 8, BLE_GREENYELLOW);
          tft.drawRect( HEADERSTATS_ICONS_X, HEADERSTATS_ICONS_Y, 8, 8, BLE_DARKGREY);
          foundFileServerIconWasRendered = true;
        }
      } else {
        if( foundFileServerIconWasRendered ) {
          tft_drawJpg(ram_jpeg,     ram_jpeg_len, HEADERSTATS_ICONS_X, HEADERSTATS_ICONS_Y,      8, 8); // heap icon
          foundFileServerIconWasRendered = false;
        }
      }
      
      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void footerStats() {
      if ( isInScroll() || isInQuery() ) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();

      #if HAS_EXTERNAL_RTC
        alignTextAt( hhmmString,   HHMM_POSX,   HHMM_POSY, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      #endif
      alignTextAt( UpTimeString, UPTIME_POSX, UPTIME_POSY, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      if( !uptimeIconWasRendered) {
        uptimeIconWasRendered = true; // only draw once
        tft_drawJpg( uptime_jpg, uptime_jpg_len, UPTIME_POSX - 16, UPTIME_POSY - 4, 12,  12);
      }
      alignTextAt("(c+) tobozo", COPYLEFT_POSX, COPYLEFT_POSY, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      #if HAS_GPS
        if( gpsIconVisible ) {
          tft_drawJpg( gps_jpg, gps_jpg_len, HHMM_POSX + 31, HHMM_POSY - 2, 10,  10);
        } else {
          tft.fillRect( HHMM_POSX + 31, HHMM_POSY - 2, 10,  10, FOOTER_BGCOLOR );
        }
      #endif
      *sessDevicesCountStr = {'\0'};
      *devicesCountStr = {'\0'};
      *newDevicesCountStr = {'\0'};

      sprintf( sessDevicesCountStr, sessDevicesCountTpl, formatUnit(sessDevicesCount) );
      sprintf( devicesCountStr, devicesCountTpl, formatUnit(devicesCount) );
      sprintf( newDevicesCountStr, newDevicesCountTpl, formatUnit(scan_rounds) );

      alignTextAt( devicesCountStr,     CDEV_C_POSX, CDEV_C_POSY, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt( sessDevicesCountStr, SESS_C_POSX, SESS_C_POSY, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      alignTextAt( newDevicesCountStr,  NDEV_C_POSX, NDEV_C_POSY, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void cacheStats() {
      takeMuxSemaphore();
      percentBox( PERCENTBOX_X, PERCENTBOX_Y - 3*(PERCENTBOX_SIZE+2)/*284*/, PERCENTBOX_SIZE, PERCENTBOX_SIZE, BLEDevCacheUsed, BLE_CYAN, BLE_BLACK);
      percentBox( PERCENTBOX_X, PERCENTBOX_Y - 2*(PERCENTBOX_SIZE+2)/*296*/, PERCENTBOX_SIZE, PERCENTBOX_SIZE, VendorCacheUsed, BLE_ORANGE, BLE_BLACK);
      percentBox( PERCENTBOX_X, PERCENTBOX_Y - 1*(PERCENTBOX_SIZE+2)/*308*/, PERCENTBOX_SIZE, PERCENTBOX_SIZE, OuiCacheUsed, BLE_GREENYELLOW, BLE_BLACK);
      if( filterVendors ) {
        tft_drawJpg( filter_jpeg, filter_jpeg_len, 152, FOOTER_BOTTOMPOS - 12/*308*/, 10,  8);
      } else {
        tft.fillRect( 152, FOOTER_BOTTOMPOS - 12/*308*/, 10,  8, FOOTER_BGCOLOR );
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
      float ratio = 10.0;
      if( w == h ) {
        ratio = w;
      } else {
        ratio = (float)w / (float)h * (float)10.0;
      }
      tft.drawRect(x - 1, y - 1, w + 2, h + 2, bordercolor);
      tft.fillRect(x, y, w, h, bgcolor);
      byte yoffsetpercent = percent / ratio;
      byte boxh = (yoffsetpercent * h) / ratio ;
      tft.fillRect(x, y, w, boxh, barcolor);

      byte xoffsetpercent = percent % (int)ratio;
      if (xoffsetpercent == 0) return;
      byte linew = (xoffsetpercent * w) / ratio;
      tft.drawFastHLine(x, y + boxh, linew, barcolor);
    }


    static void timeStateIcon() {
      tft_drawJpg( clock_jpeg, clock_jpeg_len, ICON_RTC_X-ICON_R, ICON_RTC_Y-ICON_R+1, 8,  8);
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
      tft_drawJpg( zzz_jpeg, zzz_jpeg_len, 5, 18, 8,  8 );
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

    static void PrintFatalError( const char* message, uint16_t yPos = AMIGABALL_YPOS ) {
      alignTextAt( message, 0, yPos, BLE_YELLOW, BLECARD_BGCOLOR, ALIGN_CENTER );
    }

    static void PrintProgressBar(uint16_t width) {
      if( width == Out.width || width == 0 ) { // clear
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
          tft_drawJpg( clock_jpeg, clock_jpeg_len, ICON_RTC_X-ICON_R, ICON_RTC_Y-ICON_R+1, 8,  8);
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
      //#if HAS_EXTERNAL_RTC
      xTaskCreatePinnedToCore(clockSync, "clockSync", 2048, NULL, 4, NULL, 1); // RTC wants to run on core 1 or it fails
      //#endif
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
        #else
          uptimeSet();
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
      /*
      int16_t GRAPH_LINE_WIDTH = HEAPMAP_BUFFLEN - 1;
      int16_t GRAPH_LINE_HEIGHT = 35;
      int16_t GRAPH_X = Out.width - GRAPH_LINE_WIDTH - 2;
      int16_t GRAPH_Y = FOOTER_BOTTOMPOS - 37;// 283
      */
      while (1) {

        if ( isInScroll() || isInQuery() ) {
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
      uint16_t randomcolor = tft_color565( random(128, 255), random(128, 255), random(128, 255) );
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
        tft_drawBitmap( 150, Out.scrollPosY - hop, 16, 8, macBytesToColors );
        //maclastbytes
        //tft.setTextColor( macColor, BLE_WHITE /*BLECardTheme.bgColor */);
      //}

      *dbmStr = {'\0'};
      sprintf( dbmStr, dbmTpl, BleCard->rssi );
      alignTextAt( dbmStr, 0, Out.scrollPosY - hop, BLECardTheme.textColor, BLECardTheme.bgColor, ALIGN_RIGHT );
      tft.setCursor( 0, Out.scrollPosY );
      drawRSSI( Out.width - 18, Out.scrollPosY - hop - 1, BleCard->rssi, BLECardTheme.textColor );
      if ( BleCard->in_db ) { // 'already seen this' icon
        tft_drawJpg( update_jpeg, update_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      } else { // 'just inserted this' icon
        tft_drawJpg( insert_jpeg, insert_jpeg_len, 138, Out.scrollPosY - hop, 8,  8);
      }
      if ( !isEmpty( BleCard->uuid ) ) { // 'has service UUID' Icon
        tft_drawJpg( service_jpeg, service_jpeg_len, 128, Out.scrollPosY - hop, 8,  8);
      }

      switch( BleCard->hits ) {
        case 0:
          tft_drawJpg( ghost_jpeg, ghost_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
        break;
        case 1:
          tft_drawJpg( moai_jpeg, moai_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
        break;
        default:
          tft_drawJpg( disk_jpeg, disk_jpeg_len, 118, Out.scrollPosY - hop, 8,  8);
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
    
            tft_drawJpg( clock_jpeg, clock_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
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
          tft_drawJpg( espressif_jpeg  , espressif_jpeg_len, 11, Out.scrollPosY - hop, 8, 8 );
        } else {
          tft_drawJpg( nic16_jpeg, nic16_jpeg_len, 10, Out.scrollPosY - hop, 13, 8 );
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
          tft_drawJpg( apple16_jpeg, apple16_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( BleCard->manufname, "IBM" ) ) {
          tft_drawJpg( ibm8_jpg, ibm8_jpg_len, 10, Out.scrollPosY - hop, 20,  8);
        } else if ( strstr (BleCard->manufname, "Microsoft" ) ) {
          tft_drawJpg( crosoft_jpeg, crosoft_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        } else if ( strstr( BleCard->manufname, "Bose" ) ) {
          tft_drawJpg( speaker_icon_jpg, speaker_icon_jpg_len, 12, Out.scrollPosY - hop, 6,  8 );
        } else {
          tft_drawJpg( generic_jpeg, generic_jpeg_len, 12, Out.scrollPosY - hop, 8,  8 );
        }
      }
      if ( !isEmpty( BleCard->name ) ) {
        *nameStr = {'\0'};
        sprintf(nameStr, nameTpl, BleCard->name);
        blockHeight += Out.println(SPACE);
        hop = Out.println( nameStr );
        blockHeight += hop;
        tft_drawJpg( name_jpeg, name_jpeg_len, 12, Out.scrollPosY - hop, 7,  8);
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
        Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, boxHeight, 4, tft_color565(color, color, color) );
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



  typedef enum {
    TFT_SQUARE = 0,
    TFT_PORTRAIT = 1,
    TFT_LANDSCAPE = 2
  } DisplayMode;
  
  
  DisplayMode getDisplayMode() {
    if( tft.width() > tft.height() ) {
      return TFT_LANDSCAPE;
    }
    if( tft.width() < tft.height() ) {
      return TFT_PORTRAIT;
    }
    return TFT_SQUARE;
  }
  
  // landscape / portrait theme switcher
  void setUISizePos() {
    switch( getDisplayMode() ) {
      case TFT_LANDSCAPE:
        log_w("Using UI in landscape mode (w:%d, h:%d)", Out.width, Out.height);
        HEADER_HEIGHT = 40; // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
        FOOTER_HEIGHT = 16; // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
        SCROLL_HEIGHT = ( Out.height - ( HEADER_HEIGHT + FOOTER_HEIGHT ));
        FOOTER_BOTTOMPOS    = Out.height;
        HEADERSTATS_X       = Out.width - 80;
        GRAPH_LINE_WIDTH    = HEAPMAP_BUFFLEN - 1;
        GRAPH_LINE_HEIGHT   = 30;
        GRAPH_X             = Out.width - (150);
        GRAPH_Y             = 0; // FOOTER_BOTTOMPOS - 37;// 283
        PERCENTBOX_X        = (GRAPH_X - 12); // percentbox is 10px wide + 2px margin and 2px border
        PERCENTBOX_Y        = 32;
        PERCENTBOX_SIZE     = 8;
        HEADERSTATS_ICONS_X = Out.width - (80 + 6);
        HEADERSTATS_ICONS_Y = 4;
        HEADER_LINEHEIGHT   = 16;
        PROGRESSBAR_Y       = 34;
        HHMM_POSX = 97;
        HHMM_POSY = FOOTER_BOTTOMPOS - 32;
        GPSICON_POSX = HHMM_POSX + 31;
        GPSICON_POSY = HHMM_POSY - 2;
        UPTIME_POSX = 214;
        UPTIME_POSY = FOOTER_BOTTOMPOS - 10;
        uptimeIconWasRendered = true; // never render
        COPYLEFT_POSX = 250;
        COPYLEFT_POSY = FOOTER_BOTTOMPOS - 10;
        CDEV_C_POSX = 4;
        CDEV_C_POSY = FOOTER_BOTTOMPOS - 10;
        SESS_C_POSX = 74;
        SESS_C_POSY = FOOTER_BOTTOMPOS - 10;
        NDEV_C_POSX = 144;
        NDEV_C_POSY = FOOTER_BOTTOMPOS - 10;
      break;
      case TFT_PORTRAIT:
        log_w("Using UI in portrait mode");
        HEADER_HEIGHT = 40; // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
        FOOTER_HEIGHT = 40; // Important: resulting SCROLL_HEIGHT must be a multiple of font height, default font height is 8px
        SCROLL_HEIGHT = ( Out.height - ( HEADER_HEIGHT + FOOTER_HEIGHT ));
        FOOTER_BOTTOMPOS  = Out.height;
        HEADERSTATS_X     = Out.width-112;
        GRAPH_LINE_WIDTH  = HEAPMAP_BUFFLEN - 1;
        GRAPH_LINE_HEIGHT = 35;
        GRAPH_X           = Out.width - GRAPH_LINE_WIDTH - 2;
        GRAPH_Y           = FOOTER_BOTTOMPOS - 37;// 283
        PERCENTBOX_X      = (GRAPH_X - 14); // percentbox is 10px wide + 2px margin and 2px border
        PERCENTBOX_Y      = FOOTER_BOTTOMPOS;
        PERCENTBOX_SIZE   = 10;
        HEADERSTATS_ICONS_X = 156;
        HEADERSTATS_ICONS_Y = 4;
        HEADER_LINEHEIGHT = 14;
        PROGRESSBAR_Y = 30;
        HHMM_POSX = 97;
        HHMM_POSY = FOOTER_BOTTOMPOS - 32;
        GPSICON_POSX = HHMM_POSX + 31;
        GPSICON_POSY = HHMM_POSY - 2;
        UPTIME_POSX = 97;
        UPTIME_POSY = FOOTER_BOTTOMPOS - 22;
        COPYLEFT_POSX = 77;
        COPYLEFT_POSY = FOOTER_BOTTOMPOS - 12;
        CDEV_C_POSX = 4;
        CDEV_C_POSY = FOOTER_BOTTOMPOS - 32;
        SESS_C_POSX = 4;
        SESS_C_POSY = FOOTER_BOTTOMPOS - 22;
        NDEV_C_POSX = 4;
        NDEV_C_POSY = FOOTER_BOTTOMPOS - 12;
      break;
      case TFT_SQUARE:
      default:
        log_e("Unsupported display mode");
        //uh-oh
      break;    
    }
  }

    
};


UIUtils UI;
