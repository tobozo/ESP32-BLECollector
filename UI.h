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



char *macAddressToColorStr = (char*)calloc(MAC_LEN+1, sizeof(char*));

// github avatar style mac address visual code generation \o/
// builds a 8x8 vertically symetrical matrix based on the
// bytes in the mac address, two first bytes are used to
// allocate a color, the four last bytes are drawn with
// that color
struct MacAddressColors {
  uint8_t MACBytes[8]; // 8x8
  uint16_t color;
  uint8_t scaleX, scaleY;
  size_t size;
  size_t choplevel = 0;
  MacAddressColors( const char* address, byte _scaleX, byte _scaleY ) {
    scaleX = _scaleX;
    scaleY = _scaleY;
    size = 8 * 8 * scaleX * scaleY;
    memcpy( macAddressToColorStr, address, MAC_LEN+1);
    uint8_t tokenpos = 0, val = 0, macpos = 0, msb = 0, lsb = 0;
    char *token;
    char *ptr;
    token = strtok(macAddressToColorStr, ":");
    while(token != NULL) {
      val = strtol(token, &ptr, 16);
      switch( tokenpos )  {
        case 0: msb = val; break;
        case 1: lsb = val; break;
        default:
          macpos = tokenpos-2;
          MACBytes[macpos] = val;
          MACBytes[7-macpos]= val;
        break;
      }
      tokenpos++;
      token = strtok(NULL, ":");
    }
    color = (msb*256) + lsb;
  }
  void spriteDraw( TFT_eSprite *sprite, uint16_t x, uint16_t y ) {
    sprite->setPsram( false );
    sprite->setColorDepth( 16 );
    sprite->createSprite( scaleX*8, scaleY*8 );
    sprite->setWindow( 0, 0, scaleX * 8, scaleY*8 );
    for( uint8_t i = 0; i < 8; i++ ) {
      for( uint8_t sy = 0; sy < scaleY; sy++ ) {
        for( uint8_t j = 0; j < 8; j++ ) {
          if( bitRead( MACBytes[j], i ) == 1 ) {
            sprite->pushColor( color, scaleX );
          } else {
            sprite->pushColor( BLE_WHITE, scaleX );
          }
        }
      }
    }
    sprite->pushSprite( x, y );
    sprite->deleteSprite();
  }
  void chopDraw( int32_t posx, int32_t posy, uint16_t height ) {
    if( height%scaleY != 0 || height > scaleY * 8 ) { // not a multiple !!
      log_e("Bad height request, height %d must be a multiple of scaleY %d and inferior to sizeY %d", height, scaleY, scaleY*8 );
      return;
    }
    uint8_t amount = height / scaleY;
    if( choplevel + amount > 8 || amount <= 0 ) { // out of range
      log_e("Bad height request ( i=%d; i<%d; i++)", choplevel, choplevel+amount );
      return;
    }
    tft.startWrite();
    tft.setAddrWindow( posx, posy, scaleX * 8, height );
    for( uint8_t i = choplevel; i < choplevel + amount; i++ ) {
      for( uint8_t sy = 0; sy < scaleY; sy++ ) {
        for( uint8_t j = 0; j < 8; j++ ) {
          if( bitRead( MACBytes[j], i ) == 1 ) {
            tft.pushColor( color, scaleX );
          } else {
            tft.pushColor( BLE_WHITE, scaleX );
          }
        }
      }
    }
    tft.endWrite();
    choplevel += amount;
  }
};


struct MacSwap {
  void swap(int32_t *xp, int32_t *yp) {
    uint32_t temp = *xp;
    *xp = *yp;
    *yp = temp;
  }
  bool exists( int32_t needle, int32_t *haystack, size_t haystack_size ) {
    for( size_t i=0; i< haystack_size; i++ ) {
      if( haystack[i] == needle ) return true;
    }
    return false;
  }
  int32_t hasRecentActivity( int32_t needle, int32_t *haystack, size_t haystack_size ) {
    if( haystack_size == 0 ) return true;
    for( size_t i=0; i< haystack_size; i++ ) {
      if( BLEDevRAMCache[haystack[i]]->updated_at.unixtime() < BLEDevRAMCache[needle]->updated_at.unixtime() ) return i;
    }
    return -1;
  }
  int32_t hasEnoughHits( int32_t needle, int32_t *haystack, size_t haystack_size ) {
    if( haystack_size == 0 ) return true;
    for( size_t i=0; i< haystack_size; i++ ) {
      if( BLEDevRAMCache[haystack[i]]->hits < BLEDevRAMCache[needle]->hits ) return i;
    }
    return -1;
  }
} Mac;



// load icons panel
#include "UI_Icons.h"
// then load SDutils as it uses the icons panel
#include "SDUtils.h"



class UIUtils {
  public:

    bool filterVendors = false;
    bool ScreenShotLoaded = false;
    byte brightness = BASE_BRIGHTNESS; // multiple of 8 otherwise can't turn off ^^
    byte brightnessIncrement = 8;

    struct BLECardStyle {
      uint16_t textColor = BLE_WHITE;
      uint16_t borderColor = BLE_WHITE;
      uint16_t bgColor = BLECARD_BGCOLOR;
      void setTheme( BLECardThemes themeID ) {
        switch ( themeID ) {
          case IN_CACHE_ANON:         borderColor = IN_CACHE_COLOR;     textColor = ANONYMOUS_COLOR;     break; // = 0,
          case IN_CACHE_NOT_ANON:     borderColor = IN_CACHE_COLOR;     textColor = NOT_ANONYMOUS_COLOR; break; // = 1,
          case NOT_IN_CACHE_ANON:     borderColor = NOT_IN_CACHE_COLOR; textColor = ANONYMOUS_COLOR;     break; // = 2,
          case NOT_IN_CACHE_NOT_ANON: borderColor = NOT_IN_CACHE_COLOR; textColor = NOT_ANONYMOUS_COLOR; break; // = 3
        }
        bgColor = textColor; // force transparency
      }
    };

    BLECardStyle BLECardTheme;

    void init() {
      Serial.begin(115200);
      Serial.println(welcomeMessage);
      Serial.printf("HAS BUTTONS: %s,\nHAS_XPAD: %s\nHAS PSRAM: %s\nRTC_PROFILE: %s\nHAS_EXTERNAL_RTC: %s\nHAS_GPS: %s\nTIME_UPDATE_SOURCE: %d\n",
        hasHID() ? "true" : "false",
        hasXPaxShield() ? "true" : "false",
        psramInit() ? "true" : "false",
        RTC_PROFILE,
        HAS_EXTERNAL_RTC ? "true" : "false",
        HAS_GPS ? "true" : "false",
        TIME_UPDATE_SOURCE
      );
      Serial.println("Free heap at boot: " + String(initial_free_heap));

      bool clearScreen = true;
      if (resetReason == 12) { // SW Reset
        clearScreen = false;
      }

      tft_begin();
      tft_initOrientation(); // messing with this may break the scroll
      tft_setBrightness( brightness );

      begin(); // start graph

      // make sure non-printable chars aren't printed (also disables utf8)
      tft.setAttribute( lgfx::cp437_switch, true );

      Out.init();
      setUISizePos(); // set position/dimensions for widgets and other UI items
      setIconBar(); // setup icon bar

      RGBColor colorstart = { 0x44, 0x44, 0x88 };
      RGBColor colorend   = { 0x22, 0x22, 0x44 };


/*
      struct DrawSet {
        void drawJpg( const char* name, const unsigned char *jpeg, int32_t len, uint16_t x, uint16_t y, uint16_t w, uint16_t h ) {
          Serial.printf("Rendering %s [%d*%d] at [%d, %d]\n", name, w, h, x, y);
          delay(100);
          tft.drawJpg( (const uint8_t *)jpeg, len, x, y, w, h );
        }
      } drawSet;

      // 8x8 icons
      drawSet.drawJpg(  "01", filter_jpeg,            filter_jpeg_len,            10, 10, 10, 8 );
      drawSet.drawJpg(  "02", filter_unset_jpeg,      filter_unset_jpeg_len,      10, 10, 10, 8 );
      drawSet.drawJpg(  "03", disk_jpeg,              disk_jpeg_len,              10, 10, 8,  8 );
      drawSet.drawJpg(  "04", ghost_jpeg,             ghost_jpeg_len,             10, 10, 8,  8 );
      drawSet.drawJpg(  "05", earth_jpeg,             earth_jpeg_len,             10, 10, 8,  8 );
      drawSet.drawJpg(  "06", insert_jpeg,            insert_jpeg_len,            10, 10, 8,  8 );
      drawSet.drawJpg(  "07", moai_jpeg,              moai_jpeg_len,              10, 10, 8,  8 );
      drawSet.drawJpg(  "08", ram_jpeg,               ram_jpeg_len,               10, 10, 8,  8 );
      drawSet.drawJpg(  "09", clock_jpeg,             clock_jpeg_len,             10, 10, 8,  8 );
      drawSet.drawJpg(  "10", clock3_jpeg,            clock3_jpeg_len,            10, 10, 8,  8 );
      drawSet.drawJpg(  "11", clock2_jpeg,            clock2_jpeg_len,            10, 10, 8,  8 );
      drawSet.drawJpg(  "12", zzz_jpeg,               zzz_jpeg_len,               10, 10, 8,  8 );
      drawSet.drawJpg(  "13", update_jpeg,            update_jpeg_len,            10, 10, 8,  8 );
      drawSet.drawJpg(  "14", service_jpeg,           service_jpeg_len,           10, 10, 8,  8 );
      drawSet.drawJpg(  "15", espressif_jpeg,         espressif_jpeg_len,         10, 10, 8,  8 );
      drawSet.drawJpg(  "16", apple16_jpeg,           apple16_jpeg_len,           10, 10, 8,  8 );
      drawSet.drawJpg(  "17", crosoft_jpeg,           crosoft_jpeg_len,           10, 10, 8,  8 );
      drawSet.drawJpg(  "18", generic_jpeg,           generic_jpeg_len,           10, 10, 8,  8 );
      // ?x8 icons
      drawSet.drawJpg(  "19", nic16_jpeg,             nic16_jpeg_len,             10, 10, 13, 8 );
      drawSet.drawJpg(  "20", ibm8_jpg,               ibm8_jpg_len,               10, 10, 20, 8 );
      drawSet.drawJpg(  "21", speaker_icon_jpg,       speaker_icon_jpg_len,       10, 10, 6,  8 );
      drawSet.drawJpg(  "22", name_jpeg,              name_jpeg_len,              10, 10, 7,  8 );
      drawSet.drawJpg(  "23", BLECollector_Title_jpg, BLECollector_Title_jpg_len, 10, 10, 82, 8 );
      // ?x? icons
      drawSet.drawJpg(  "24", ble_jpeg,               ble_jpeg_len,               10, 10, 7,  11 );
      drawSet.drawJpg(  "25", db_jpeg,                db_jpeg_len,                10, 10, 12, 11 );
      drawSet.drawJpg(  "26", tbz_28x28_jpg,          tbz_28x28_jpg_len,          10, 10, 28, 28 );
      drawSet.drawJpg(  "27", disk00_jpg,             disk00_jpg_len,             10, 10, 30, 30 );
      drawSet.drawJpg(  "28", disk01_jpg,             disk01_jpg_len,             10, 10, 30, 30 );
      drawSet.drawJpg(  "29", gps_jpg,                gps_jpg_len,                10, 10, 10, 10 );
      drawSet.drawJpg(  "30", nogps_jpg,              nogps_jpg_len,              10, 10, 10, 10 );

      while(1) {
        ;
      }
      */

      if (clearScreen) {
        tft.fillScreen(BLE_BLACK);
        tft_fillGradientHRect( 0, headerHeight, Out.width/2, scrollHeight, colorstart, colorend );
        tft_fillGradientHRect( Out.width/2, headerHeight, Out.width/2, scrollHeight, colorend, colorstart );
        // clear heap map
        for (uint16_t i = 0; i < heapMapBuffLen; i++) heapmap[i] = 0;
      }
      tft.fillRect(0, 0, Out.width, headerHeight, HEADER_BGCOLOR);// fill header
      tft.fillRect(0, footerBottomPosY - footerHeight, Out.width, footerHeight, FOOTER_BGCOLOR);// fill footer
      tft.fillRect(0, progressBarY, Out.width, 2, BLE_GREENYELLOW);// fill progressbar
      if( footerHeight > 16 ) { // fill bottom decoration unless in landscape mode
        tft.fillRect(0, footerBottomPosY - (footerHeight - 2), Out.width, 1, BLE_DARKGREY);
      }
      IconRender( Icon8h_BLECollector_src, 2, 3 );
      IconRender( Icon_ble_src, 91, 2 );
      IconRender( Icon_db_src, 104, 2 );

/*
      uint16_t xpos = 0;
      uint16_t ypos = 50;
      for( byte a=10 ; a<128; a++ ) {
        drawBluetoothLogo( xpos, ypos, a );
        xpos +=a+1;
        if( xpos > tft.width() ) {
          ypos += a+1;
          xpos = 0;
        }
        if( ypos > tft.height() ) {
          break;
        }
      }
      while(1) { ; }
*/
      alignTextAt( COPYLEFT_SIGN " " AUTHOR , copyleftPosX, copyleftPosY, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );

      if (resetReason == 12) { // SW Reset
        headerStats("Rebooted");
      } else {
        headerStats("Init SD");
      }

      Out.setupScrollArea( headerHeight, footerHeight, colorstart, colorend );

      SDSetup();
      timeSetup();
      SetTimeStateIcon();
      footerStats();
      cacheStats();
      BLECollectorIconBar.draw( BLECollectorIconBarX, BLECollectorIconBarY );
      if ( clearScreen ) {
        playIntro();
      } else {
        Out.scrollNextPage();
      }
    }

    void begin() {
      // alloc some ram for the heap graph
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

      for (int i = 0; i < 5; i++) {
        pos += Out.println();
      }
      const char* introTextTitle = PLATFORM_NAME " BLE Collector";
      tft_getTextBounds(introTextTitle, Out.scrollPosX, Out.scrollPosY, &Out.x1_tmp, &Out.y1_tmp, &Out.w_tmp, &Out.h_tmp);
      uint16_t boxWidth = Out.w_tmp + 24;
      pos += Out.println();
      alignTextAt( introTextTitle, 6, Out.scrollPosY-Out.h_tmp, BLE_GREENYELLOW, BLE_TRANSPARENT/*BLECARD_BGCOLOR*/, ALIGN_CENTER );
      pos += Out.println();
      pos += Out.println();
      alignTextAt( COPYLEFT_SIGN "  " AUTHOR "  2019", 6, Out.scrollPosY-Out.h_tmp, BLE_GREENYELLOW, BLE_TRANSPARENT, ALIGN_CENTER );
      //pos += Out.println();
      pos += Out.println();
      pos += Out.println();

      IconRender( Icon_tbz_src, (Out.width/2 - 14), Out.scrollPosY - pos + 8 );

      Out.drawScrollableRoundRect( (Out.width/2 - boxWidth/2), Out.scrollPosY-pos, boxWidth, pos, 8, BLE_GREENYELLOW );

      for (int i = 0; i < 5; i++) {
        Out.println();
      }

      giveMuxSemaphore();
      delay(2000);
      takeMuxSemaphore();
      Out.scrollNextPage();
      giveMuxSemaphore();
      xTaskCreatePinnedToCore(introUntilScroll, "introUntilScroll", 2048, NULL, 8, NULL, 0);
    }


    static void screenShot() {

      takeMuxSemaphore();
      isQuerying = true;

      /*
      if( !ScreenShotLoaded ) {
        M5.ScreenShot.init( &tft, BLE_FS );
        M5.ScreenShot.begin();
        ScreenShotLoaded = true;
      }*/

      int16_t yRef = Out.yRef - Out.scrollTopFixedArea;
      // match pixel copy area with scroll area
      tft.setScrollRect(0, Out.scrollTopFixedArea, Out.width, Out.yArea);
      tft_hScrollTo( Out.scrollTopFixedArea ); // reset hardware scroll position before capturing
      tft_scrollTo( -yRef ); // reverse software-scroll to compensate hardware scroll offset

      //M5.ScreenShot.snapBMP("BLECollector", false);
      //M5.ScreenShot.snapJPG("BLECollector", false);
      M5.ScreenShot.snap("BLECollector", false); // filename prefix, show image after capture

      // restore scroll states
      tft_scrollTo( yRef ); // restore software scroll
      tft_hScrollTo( Out.yRef ); // restore hardware scroll

      isQuerying = false;
      giveMuxSemaphore();

    }

    static void screenShow( void * fileName = NULL ) {

      if( fileName == NULL ) return;
      isQuerying = true;

      // reset hardware scroll position before printing
      tft_hScrollTo( Out.scrollTopFixedArea );

      if( String( (const char*)fileName ).endsWith(".jpg" ) ) {
        if( !BLE_FS.exists( (const char*)fileName ) ) {
          log_e("File %s does not exist\n", (const char*)fileName );
        } else {
          takeMuxSemaphore();
          Out.scrollNextPage(); // reset scroll position to zero otherwise image will have offset
          tft.drawJpgFile( BLE_FS, (const char*)fileName, 0, 0, Out.width, Out.height, 0, 0, JPEG_DIV_NONE );
          giveMuxSemaphore();
          vTaskDelay( 5000 );
        }
      }
      if( String( (const char*)fileName ).endsWith(".bmp" ) ) {
        if( !BLE_FS.exists( (const char*)fileName ) ) {
          log_e("File %s does not exist\n", (const char*)fileName );
        } else {
          takeMuxSemaphore();
          Out.scrollNextPage(); // reset scroll position to zero otherwise image will have offset
          tft.drawBmpFile( BLE_FS, (const char*)fileName, 0, 0 );
          giveMuxSemaphore();
          vTaskDelay( 5000 );
        }
      }

      isQuerying = false;
    }


    static void introUntilScroll( void * param ) {
      char randomAddressStr[18] = {0};
      uint8_t randomAddress[6] = {0,0,0,0,0,0};
      uint16_t x;
      uint16_t y;
      size_t counter = 0;
      while( counter++ < 30 ) {
        for( byte i = 0; i<6 ; i++ ) {
          randomAddress[i] = random(0,255);
        }
        sprintf(randomAddressStr, "%02x:%02x:%02x:%02x:%02x:%02x",
                randomAddress[0],
                randomAddress[1],
                randomAddress[2],
                randomAddress[3],
                randomAddress[4],
                randomAddress[5]
        );
        log_d("Generated fake mac: %s", randomAddressStr);
        x = hallOfMacPosX + (counter%hallofMacCols) * hallOfMacItemWidth;
        y = hallOfMacPosY + ((counter/hallofMacCols)%hallofMacRows) * hallOfMacItemHeight;
        MacAddressColors AvatarizedMAC( randomAddressStr, 2, 1 );
        takeMuxSemaphore();
        AvatarizedMAC.spriteDraw( &animSprite, hallOfMacHmargin + x, hallOfMacVmargin + y );
        giveMuxSemaphore();
        counter++;
        vTaskDelay(30);
      }
      vTaskDelete(NULL);
    }


    static void headerStats(const char *status = "") {
      if ( isInScroll() || isInQuery() ) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();
      int16_t statuspos = 0;
      if ( !isEmpty( status ) ) {
        uint8_t alignoffset = leftMargin;
        tft.fillRect(0, headerStatsIconsY + headerLineHeight, iconAppX, 8, HEADER_BGCOLOR); // clear whole message status area
        if (strstr(status, "Inserted")) {
          IconRender( TextCounters_heap_src, alignoffset,   headerStatsIconsY + headerLineHeight ); // disk icon
          alignoffset +=10;
        } else if (strstr(status, "Cache")) {
          IconRender( TextCounters_entries_src, alignoffset,   headerStatsIconsY + headerLineHeight ); // ghost icon
          alignoffset +=10;
        } else if (strstr(status, "DB")) {
          IconRender( TextCounters_scans_src, alignoffset,   headerStatsIconsY + headerLineHeight ); // moai icon
          alignoffset +=10;
        } else if (strstr(status, "Scan")) {
          IconRender( Icon8x8_zzz_src, alignoffset,   headerStatsIconsY + headerLineHeight ); // sleep icon
          alignoffset +=10;
        }
        alignTextAt( status, alignoffset, headerStatsIconsY + headerLineHeight, BLE_YELLOW, HEADER_BGCOLOR, ALIGN_FREE );
        statuspos = Out.x1_tmp + Out.w_tmp;
      }
      if( !appIconRendered || statuspos > iconAppX ) { // only draw if text has overlapped
        IconRender( Icon_tbz_src, iconAppX, iconAppY ); // app icon
        appIconRendered = true;
      }

      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void footerStats() {
      if ( isInScroll() || isInQuery() ) return;
      takeMuxSemaphore();
      int16_t posX = tft.getCursorX();
      int16_t posY = tft.getCursorY();

      if( TimeIsSet ) {
        alignTextAt( hhmmString, hhmmPosX, hhmmPosY, BLE_YELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      }
      //alignTextAt( UpTimeString, uptimePosX, uptimePosY, BLE_GREENYELLOW, FOOTER_BGCOLOR, ALIGN_FREE );
      tft.setCursor(posX, posY);
      giveMuxSemaphore();
    }


    void cacheStats() {
      takeMuxSemaphore();
      percentBox( percentBoxX, percentBoxY - 3*(percentBoxSize+2), percentBoxSize, percentBoxSize, BLEDevCacheUsed, BLE_CYAN,        BLE_BLACK);
      percentBox( percentBoxX, percentBoxY - 2*(percentBoxSize+2), percentBoxSize, percentBoxSize, VendorCacheUsed, BLE_ORANGE,      BLE_BLACK);
      percentBox( percentBoxX, percentBoxY - 1*(percentBoxSize+2), percentBoxSize, percentBoxSize, OuiCacheUsed,    BLE_GREENYELLOW, BLE_BLACK);
      giveMuxSemaphore();
    }


    static void startBlink() { // runs one and detaches
      blinkit = true;
      blinknow = millis();
      scanTime = SCAN_DURATION * 1000;
      blinkthen = blinknow + scanTime;
      lastblink = millis();
      lastprogress = millis();
    }


    static void stopBlink() {
      blinkit = false;
      int32_t totalrssi = 0;
      size_t count      = 0;
      for(uint16_t i=0; i<MAX_DEVICES_PER_SCAN; i++) {
        if( BLEDevScanCache[i]->rssi !=0 ) {
          totalrssi += BLEDevScanCache[i]->rssi;
          count++;
        }
      }
      if( count > 0 ) {
        BLERssiWidget.setValue ( totalrssi / count );
      }
    }

    // sqlite state (read/write/inert) icon
    static void SetDBStateIcon(int state) {
      switch (state) {
        case 2:/*DB OPEN FOR WRITING*/ DBIcon.setStatus( ICON_STATUS_DB_WRITE ); break;
        case 1:/*DB_OPEN FOR READING*/ DBIcon.setStatus( ICON_STATUS_DB_READ );  break;
        case 0:/*DB CLOSED*/           DBIcon.setStatus( ICON_STATUS_DB_IDLE );  break;
        case -1:/*DB BROKEN*/          DBIcon.setStatus( ICON_STATUS_DB_ERROR ); break;
        default:/*DB INACTIVE*/        DBIcon.setStatus( ICON_STATUS_DB_IDLE );  break;
      }
    }

    static void PrintFatalError( const char* message, uint16_t yPos = AMIGABALL_YPOS ) {
      alignTextAt( message, 0, yPos, BLE_YELLOW, BLECARD_BGCOLOR, ALIGN_CENTER );
    }
    static void PrintProgressBar(float progress, float magnitude) {
      PrintProgressBar( (Out.width * progress) / magnitude );
    }

    static void PrintProgressBar(uint16_t width) {
      if( width > Out.width || width == 0 ) { // clear
        tft.fillRect(0,     progressBarY, Out.width, 2, BLE_DARKGREY);
      } else {
        tft.fillRect(0,     progressBarY, width,           2, BLUETOOTH_COLOR);
        tft.fillRect(width, progressBarY, Out.width-width, 2, BLE_DARKGREY);
      }
    }

    static void SetTimeStateIcon() {
      if (RTCisRunning) {
        TimeIcon.setStatus( ICON_STATUS_clock3 );
      } else {
        if( TimeIsSet ) {
          TimeIcon.setStatus( ICON_STATUS_clock );
        } else {
          TimeIcon.setStatus( ICON_STATUS_clock2 );
        }
      }
      if( GPSHasDateTime ) {
        GPSIcon.setStatus( ICON_STATUS_gps );
      } else{
        GPSIcon.setStatus( ICON_STATUS_nogps );
      }
    }


    static void PrintBlinkableWidgets() {
      if (!blinkit || blinknow >= blinkthen) {
        blinkit = false;
        if( BLEActivityIcon.status != ICON_STATUS_IDLE ) {
          takeMuxSemaphore();
          PrintProgressBar( 0 );
          giveMuxSemaphore();
          BLEActivityIcon.setStatus( ICON_STATUS_IDLE );
        }
        return;
      }

      blinknow = millis();
      if (lastblink + random(222, 666) < blinknow) {
        blinktoggler = !blinktoggler;
        if (blinktoggler) {
          if( foundFileServer || foundTimeServer ) {
            BLEActivityIcon.setStatus( ICON_STATUS_ADV_WHITELISTED );
          } else {
            BLEActivityIcon.setStatus( ICON_STATUS_ADV_SCAN );
          }
        } else {
          BLEActivityIcon.setStatus( ICON_STATUS_IDLE );
        }
        lastblink = blinknow;
      }

      if (lastprogress + 1000 < blinknow) {
        unsigned long remaining = blinkthen - blinknow;
        int percent = 100 - ( ( remaining * 100 ) / scanTime );
        takeMuxSemaphore();
        PrintProgressBar( (Out.width * percent) / 100 );
        giveMuxSemaphore();
        lastprogress = blinknow;
      }
    }

    // spawn subtasks and leave
    static void taskHeapGraph( void * pvParameters ) { // always running
      mux = xSemaphoreCreateMutex();
      takeMuxSemaphore();
      for( uint16_t i = 0; i < hallOfMacSize; i++ ) {
        uint16_t x = hallOfMacPosX + (i%hallofMacCols) * hallOfMacItemWidth;
        uint16_t y = hallOfMacPosY + ((i/hallofMacCols)%hallofMacRows) * hallOfMacItemHeight;
        animClear( x, y, hallOfMacItemWidth, hallOfMacItemHeight, FOOTER_BGCOLOR, BLE_WHITE );
      }
      heapGraphSprite.setPsram( false );
      heapGraphSprite.setColorDepth( 16 );
      heapGraphSprite.createSprite( graphLineWidth, graphLineHeight );
      giveMuxSemaphore();

      xTaskCreatePinnedToCore(clockSync, "clockSync", 2048, NULL, 2, NULL, 1); // RTC wants to run on core 1 or it fails
      xTaskCreatePinnedToCore(drawableItems, "drawableItems", 6144, NULL, 2, NULL, 1);
      vTaskDelete(NULL);
    }


    static void drawableItems( void * param ) {
      while(1) {
        if( freeheap != lastfreeheap ) {
          takeMuxSemaphore();
          heapmap[heapindex++] = freeheap;
          heapindex = heapindex % heapMapBuffLen;
          lastfreeheap = freeheap;
          giveMuxSemaphore();
        }
        PrintBlinkableWidgets();
        BLECollectorIconBar.draw( BLECollectorIconBarX, BLECollectorIconBarY );
        vTaskDelay( 100 );
      }
    }

    static void hallOfMac( int32_t * sorted, int32_t * lastsorted ) {
      // get the 8 top hits in the cache
      size_t macFound = 0;
      int16_t index = BLEDEVCACHE_SIZE-1;
      uint16_t x;
      uint16_t y;

      while( index >= 0 ) {
        if( isEmpty( BLEDevRAMCache[index]->address ) || BLEDevRAMCache[index]->hits == 0 ) {
          index--;
          continue;
        }
        if( !Mac.exists( index, sorted, macFound ) ) { // not in list
          if( macFound < hallOfMacSize ) {
            sorted[macFound] = index;
            macFound++;
          } else {
            int32_t has = Mac.hasRecentActivity( index, sorted, macFound );
            if( has > -1 ) { // insertable
              sorted[has] = index;
            }
          }
        }
        index--;
      }
      if( macFound > 1 ) {
        // bubble sort by hits
        for( uint16_t i = 0; i < macFound-1; i++ ) {
          for ( uint16_t j = 0; j < macFound-i-1; j++ ) {
            if( BLEDevRAMCache[sorted[j]]->hits < BLEDevRAMCache[sorted[j+1]]->hits ) {
              Mac.swap(&sorted[j], &sorted[j+1]);
            }
          }
        }
      }
      if( macFound > 0 ) {
        for( uint16_t i = 0; i < hallOfMacSize; i++ ) {
          x = hallOfMacPosX + (i%hallofMacCols) * hallOfMacItemWidth;
          y = hallOfMacPosY + ((i/hallofMacCols)%hallofMacRows) * hallOfMacItemHeight;
          if( i<macFound ) {
            if( lastsorted[i] != sorted[i] ) {
              takeMuxSemaphore();
              // cleanup current slot
              animClear( x, y, hallOfMacItemWidth, hallOfMacItemHeight, FOOTER_BGCOLOR, BLE_WHITE );
              // draw current slot
              MacAddressColors AvatarizedMAC( BLEDevRAMCache[sorted[i]]->address, 2, 1 );
              AvatarizedMAC.spriteDraw( &animSprite, hallOfMacHmargin + x, hallOfMacVmargin + y );
              giveMuxSemaphore();
            }
          } else {
            if( lastsorted[i] > 0 ) {
              takeMuxSemaphore();
              //tft.fillRect( hallOfMacHmargin + x, hallOfMacVmargin + y, hallOfMacItemWidth, hallOfMacItemHeight, FOOTER_BGCOLOR );
              animClear( x, y, hallOfMacItemWidth, hallOfMacItemHeight, FOOTER_BGCOLOR, BLE_WHITE );
              giveMuxSemaphore();
            }
          }
        }
        //Serial.println();
      }
    }

    static void textCounters() {
      if( !showScanStats ) {
        unsigned long timer_sec = (millis()/3000);
        showHeap    = timer_sec%5==0;
        showEntries = timer_sec%5==1;
        showSessc   = timer_sec%5==2;
        showNdevc   = timer_sec%5==3;
        showUptime  = timer_sec%5==4;
        *textWidgetStr = {0};
      }
      //ICON_STATUS_HEAP, ICON_STATUS_ENTRIES, ICON_STATUS_LAST, ICON_STATUS_SEEN, ICON_STATUS_SCANS, ICON_STATUS_UPTIME
      if( showHeap ) {
        sprintf( textWidgetStr, heapTpl, freeheap);
        TextCountersIcon.status = ICON_STATUS_HEAP;
        TextCountersWidget.setText( textWidgetStr, heapStrX, heapStrY, BLE_GREENYELLOW, HEADER_BGCOLOR, heapAlign );
      }
      if( showEntries ) {
        sprintf( textWidgetStr, entriesTpl, formatUnit(entries));
        TextCountersIcon.status = ICON_STATUS_ENTRIES;
        TextCountersWidget.setText( textWidgetStr, entriesStrX, entriesStrY, BLE_GREENYELLOW, HEADER_BGCOLOR, entriesAlign );
      }
      if( showSessc ) {
        sprintf( textWidgetStr, seenDevicesCountTpl, seenDevicesCountSpacer, formatUnit(sessDevicesCount) );
        TextCountersIcon.status = ICON_STATUS_SEEN;
        TextCountersWidget.setText( textWidgetStr, sesscPosX, sesscPosY, BLE_YELLOW, FOOTER_BGCOLOR, sesscAlign );
      }
      if( showNdevc ) {
        sprintf( textWidgetStr, scansCountTpl, scansCountSpacer, formatUnit(scan_rounds) );
        TextCountersIcon.status = ICON_STATUS_SCANS;
        TextCountersWidget.setText( textWidgetStr, ndevcPosX, ndevcPosY, BLE_GREENYELLOW, FOOTER_BGCOLOR, ndevcAlign );
      }
      if( showUptime ) {
        sprintf( textWidgetStr, UpTimeStringTplTpl, UpTimeString );
        TextCountersIcon.status = ICON_STATUS_UPTIME;
        TextCountersWidget.setText( textWidgetStr, uptimePosX, uptimePosY, BLE_GREENYELLOW, FOOTER_BGCOLOR, uptimeAlign );
      }
    }


    static void devicesGraphStats() {
        devGraphStartedSince = millis() - devGraphFirstStatTime;
        devCountPerMinuteIndex = int( devGraphStartedSince / devGraphPeriodShort )%60;
        devCountPerMinute[devCountPerMinuteIndex] = devicesStatCount;
        devicesStatCount = 0;

        mincdpm = 0xffff;
        maxcdpm = 0;
        for ( uint8_t i = 0; i < 60; i++ ) {
          if( devCountPerMinute[i] > maxcdpm ) {
            maxcdpm = devCountPerMinute[i];
          }
          if( devCountPerMinute[i] != 0 && devCountPerMinute[i] < mincdpm ) {
            mincdpm = devCountPerMinute[i];
          }
        }

        if( devGraphStartedSince > devGraphPeriodLong && devCountPerMinuteIndex%(devGraphPeriodLong/1000) == 0 ) { // every 2s
          // 1mn of data => calc devices per minute per minute
          size_t totalCount = 0;
          for( uint8_t m=0; m<60; m++ ) {
            totalCount += devCountPerMinute[m];
          }
          devCountPerMinutePerPeriodIndex = int( devGraphStartedSince / devGraphPeriodLong ) % graphLineWidth;
          devCountPerMinutePerPeriod[devCountPerMinutePerPeriodIndex] = totalCount;
          mincdpmpp = 0xffff;
          maxcdpmpp = 0;
          for ( uint8_t i = 0; i < 60; i++ ) {
            if( devCountPerMinutePerPeriod[i] > maxcdpmpp ) {
              maxcdpmpp = devCountPerMinutePerPeriod[i];
            }
            if( devCountPerMinutePerPeriod[i] != 0 && devCountPerMinutePerPeriod[i] < mincdpmpp ) {
              mincdpmpp = devCountPerMinutePerPeriod[i];
            }
          }
          log_v( "%d devices per minute per %d seconds  (%.2f), min(%d), max(%d)", totalCount, (devGraphPeriodLong/1000), (float)totalCount/60, mincdpmpp, maxcdpmpp );
        }
        devCountWasUpdated = true;
    }


    static void clockSync(void * parameter) {

      TickType_t lastWaketime;
      lastWaketime = xTaskGetTickCount();
      devGraphFirstStatTime = millis();

      int32_t *sorted     = (int32_t*)malloc( sizeof(int32_t) * hallOfMacSize );
      int32_t *lastsorted = (int32_t*)malloc( sizeof(int32_t) * hallOfMacSize );

      for( uint16_t i = 0; i< hallOfMacSize; i++ ) {
        sorted[i]     = -1;
      }

      while(1) {
        for( uint16_t i = 0; i < hallOfMacSize; i++ ) {
          lastsorted[i] = sorted[i];
        }

        if( TimeIsSet ) {
          takeMuxSemaphore();
          timeHousekeeping();
          giveMuxSemaphore();
        } else {
          uptimeSet();
        }

        SetTimeStateIcon();
        hallOfMac( sorted, lastsorted );
        textCounters();
        devicesGraphStats();
        heapGraph();
        // make sure it happens at least once every 1000 ms
        vTaskDelayUntil(&lastWaketime, 1000 / portTICK_PERIOD_MS);
      }
    }


    static void heapGraph() {
      if ( isInScroll() || isInQuery() ) {
        return;
      }
      if( ! devCountWasUpdated ) {
        return;
      }
      devCountWasUpdated = false;

      // render heatmap
      uint16_t GRAPH_COLOR = BLE_WHITE;
      uint32_t graphMin = min_free_heap;
      uint32_t graphMax = graphMin;
      uint32_t toleranceline = graphLineHeight;
      uint32_t minline = 0;
      uint16_t GRAPH_BG_COLOR = BLE_BLACK;
      uint16_t currentheapindex = heapindex;
      // dynamic scaling
      for (uint8_t i = 0; i < graphLineWidth; i++) {
        int thisindex = int(currentheapindex - graphLineWidth + i + heapMapBuffLen) % heapMapBuffLen;
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
        return;
      }
      // bounds, min and max lines
      minline = map(min_free_heap, graphMin, graphMax, 0, graphLineHeight);
      if (toleranceheap > graphMax) {
        GRAPH_BG_COLOR = BLE_ORANGE;
        toleranceline = graphLineHeight;
      } else if ( toleranceheap < graphMin ) {
        toleranceline = 0;
      } else {
        toleranceline = map(toleranceheap, graphMin, graphMax, 0, graphLineHeight);
      }
      // draw graph

      heapGraphSprite.fillSprite( BLE_BLACK );

      for (uint8_t i = 0; i < graphLineWidth; i++) {
        int thisindex = int(currentheapindex - graphLineWidth + i + heapMapBuffLen) % heapMapBuffLen;
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
        heapGraphSprite.drawFastVLine( i, 0, graphLineHeight, GRAPH_BG_COLOR );
        if ( heapval > 0 ) {
          uint32_t lineheight = map(heapval, graphMin, graphMax, 0, graphLineHeight);
          heapGraphSprite.drawFastVLine( i, graphLineHeight-lineheight, lineheight, GRAPH_COLOR );
        }
      }
      heapGraphSprite.drawFastHLine( 0, graphLineHeight - toleranceline, graphLineWidth, BLE_LIGHTGREY );
      heapGraphSprite.drawFastHLine( 0, graphLineHeight - minline, graphLineWidth, BLE_RED );

      if( devGraphStartedSince > devGraphPeriodLong ) {
        // 1mn of data => calc devices per minute
        size_t totalCount = 0;
        for( uint8_t m=0; m<60; m++ ) {
          totalCount += devCountPerMinute[m];
        }

        dcpmLastX  = 0;
        dcpmLastY  = dcpmFirstY;

        dcpmppLastX  = 0;
        dcpmppLastY  = 0;

        for (uint8_t i = 0; i < graphLineWidth; i++) {
          uint16_t coordX = /*graphX +*/ i;
          uint16_t coordY = 0;

          uint8_t perMinuteIndex = map( (devCountPerMinuteIndex+i+1)%60, 0, graphLineWidth, 0, 60 ); // map to X
          int16_t dcpm = map( devCountPerMinute[perMinuteIndex], mincdpm, maxcdpm, 0, (graphLineHeight/4)-2); // map to Y
          coordY = baseCoordY - dcpm;
          if( devCountPerMinute[perMinuteIndex] > 0 ) {
            if( i==0 ) {
              dcpmFirstY = coordY;
            } else {
              heapGraphSprite.drawLine( coordX, coordY, dcpmLastX, dcpmLastY, BLE_DARKBLUE );
              dcpmLastX = coordX;
            }
            dcpmLastY = coordY;
          }

          uint8_t perMinutePerPeriodIndex = (devCountPerMinutePerPeriodIndex+i);
          perMinutePerPeriodIndex++;
          perMinutePerPeriodIndex = perMinutePerPeriodIndex%graphLineWidth; // map to X
          int16_t dcpmpp = map( devCountPerMinutePerPeriod[perMinutePerPeriodIndex], mincdpmpp, maxcdpmpp, (graphLineHeight/4)+2, graphLineHeight*.75); // map to Y
          coordY = baseCoordY - dcpmpp;
          if( devCountPerMinutePerPeriod[perMinutePerPeriodIndex] > 0 ) {
            if( dcpmppLastY > 0 ) {
              heapGraphSprite.drawLine( coordX, coordY, dcpmppLastX, dcpmppLastY, BLE_DARKBLUE );
            }
            dcpmppLastX = coordX;
            dcpmppLastY = coordY;
          }

        }
        // join last/first
        heapGraphSprite.drawLine( dcpmLastX, dcpmLastY, graphLineWidth, dcpmFirstY, BLE_DARKBLUE );
      }
      takeMuxSemaphore();
      heapGraphSprite.pushSprite( graphX, graphY );
      giveMuxSemaphore();
    }


    void printBLECard( BlueToothDeviceLink BleLink /*BlueToothDevice *BleCard*/ ) {
      //unsigned long renderstart = millis();
      BlueToothDevice *BleCard = BleLink.device;
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
      uint16_t blockHeight = 0;
      uint16_t hop;
      uint16_t initialPosY = Out.scrollPosY;
      MacAddressColors AvatarizedMAC( BleCard->address, macAddrColorsScaleX, macAddrColorsScaleY );

      *addressStr = {'\0'};
      sprintf( addressStr, addressTpl, BleCard->address );
      *dbmStr = {'\0'};
      sprintf( dbmStr, dbmTpl, BleCard->rssi );

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

      hop = Out.println( addressStr );
      blockHeight += hop;

      alignTextAt( dbmStr, 0, Out.scrollPosY - hop, BLECardTheme.textColor, BLECardTheme.bgColor, ALIGN_RIGHT );
      tft.setCursor( 0, Out.scrollPosY );
      drawRSSIBar( Out.width - 18, Out.scrollPosY - hop - 1, BleCard->rssi, BLECardTheme.textColor );
      if ( BleCard->in_db ) { // 'already seen this' icon
        IconRender( Icon8x8_update_src, 138, Out.scrollPosY - hop );
      } else { // 'just inserted this' icon
        IconRender( TextCounters_seen_src, 138, Out.scrollPosY - hop );
      }
      if ( !isEmpty( BleCard->uuid ) ) { // 'has service UUID' Icon
        IconRender( Icon8x8_service_src, 128, Out.scrollPosY - hop );
      }

      switch( BleCard->hits ) {
        case 0:
          IconRender( TextCounters_entries_src, 118, Out.scrollPosY - hop );
        break;
        case 1:
          IconRender( TextCounters_scans_src, 118, Out.scrollPosY - hop );
        break;
        default:
          IconRender( TextCounters_heap_src, 118, Out.scrollPosY - hop );
        break;
      }


      if( TimeIsSet ) {
        if ( BleCard->hits > 1 ) {
          *hitsStr = {'\0'};
          sprintf(hitsStr, "(%s hits)", formatUnit( BleCard->hits ) );

          if( BleCard->updated_at.unixtime() > 0 /* BleCard->created_at.year() > 1970 */) {
            /*
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
            IconRender( TimeIcon_SET_src, 12, Out.scrollPosY - hop );
          }
        }
      }

      if( !isEmpty( BleCard->uuid ) ) {

        //BLEUUID tmpID;
        //tmpID.fromString(BleCard->uuid);
        BLEGATTService srv = BLEDevHelper.gattServiceDescription( BleCard->uuid );
        //const char* serviceStr = BLEDevHelper.gattServiceDescription( tmpID );

        // TODO: icon render
        if( strcmp( srv.name, "Unknown" ) != 0 ) {
          blockHeight += Out.println( SPACE );
          *ouiStr = {'\0'};
          sprintf( ouiStr, ouiTpl, srv.name );
          hop = Out.println( ouiStr );
          blockHeight += hop;
        }
      }

      if ( !isEmpty( BleCard->ouiname ) ) {
        blockHeight += Out.println( SPACE );
        *ouiStr = {'\0'};
        sprintf( ouiStr, ouiTpl, BleCard->ouiname );
        hop = Out.println( ouiStr );
        blockHeight += hop;
        if ( strstr( BleCard->ouiname, "Espressif" ) ) {
          IconRender( Icon8x8_espressif_src, 11, Out.scrollPosY - hop );
        } else {
          IconRender( Icon8h_nic16_src, 10, Out.scrollPosY - hop );
        }
      }

      bool jumpNext = true;
      if( macAddrColorsSizeY <= Out.h_tmp ) {
        AvatarizedMAC.chopDraw( macAddrColorsPosX, Out.scrollPosY - hop, macAddrColorsSizeY );
      } else {
        // chop-chop!
        int16_t sizeY = macAddrColorsSizeY;
        while( sizeY >= Out.h_tmp ) {
          sizeY -= Out.h_tmp;
          AvatarizedMAC.chopDraw( macAddrColorsPosX, Out.scrollPosY - hop, Out.h_tmp );
          if( sizeY >= Out.h_tmp ) {
            blockHeight += Out.println(SPACE);
          }
        }
        jumpNext = false;
      }
      if ( BleCard->appearance != 0 ) {
        if( jumpNext ) {
          blockHeight += Out.println(SPACE);
        } else {
          jumpNext = true;
        }
        *appearanceStr = {'\0'};
        sprintf( appearanceStr, appearanceTpl, BleCard->appearance );
        hop = Out.println( appearanceStr );
        blockHeight += hop;
      }
      if ( !isEmpty( BleCard->manufname ) ) {
        if( jumpNext ) {
          blockHeight += Out.println(SPACE);
        } else {
          jumpNext = true;
        }
        *manufStr = {'\0'};
        sprintf( manufStr, manufTpl, BleCard->manufname );
        hop = Out.println( manufStr );
        blockHeight += hop;
        if ( strstr( BleCard->manufname, "Apple" ) ) {
          IconRender( Icon8x8_apple16_src, 12, Out.scrollPosY - hop );
        } else if ( strstr( BleCard->manufname, "IBM" ) ) {
          IconRender( Icon8h_ibm8_src, 10, Out.scrollPosY - hop );
        } else if ( strstr (BleCard->manufname, "Microsoft" ) ) {
          IconRender( Icon8x8_crosoft_src, 12, Out.scrollPosY - hop );
        } else if ( strstr( BleCard->manufname, "Bose" ) ) {
          IconRender( Icon8h_speaker_src, 12, Out.scrollPosY - hop );
        } else {
          IconRender( Icon8x8_generic_src, 12, Out.scrollPosY - hop );
        }
      }
      if ( !isEmpty( BleCard->name ) ) {
        *nameStr = {'\0'};
        sprintf(nameStr, nameTpl, BleCard->name);
        if( jumpNext ) {
          blockHeight += Out.println(SPACE);
        } else {
          jumpNext = true;
        }
        hop = Out.println( nameStr );
        blockHeight += hop;
        IconRender( Icon8h_name_src, 12, Out.scrollPosY - hop );
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
      MacScrollView[lastPrintedMacIndex].cacheIndex = BleLink.cacheIndex;
      giveMuxSemaphore();

      //unsigned long rendertime = millis() - renderstart;
      //log_w("Rendered %s in %d ms", BleCard->address, rendertime );

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
            highlightBLECard( card_index, -offset );
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

    static void highlightBLECard( uint16_t card_index, int16_t offset ) {
      if( card_index >= BLECARD_MAC_CACHE_SIZE) return; // bad value
      if( isEmpty( MacScrollView[card_index].address ) ) return; // empty slot
      int newYPos = Out.translate( Out.scrollPosY, offset );
      headerStats( MacScrollView[card_index].address );
      takeMuxSemaphore();
      uint16_t boxHeight = MacScrollView[card_index].blockHeight-2;
      uint16_t boxWidth  = Out.width - 2;
      uint16_t boxPosY   = newYPos + 1;
      for( int16_t color=255; color>64; color-=4 ) {
        Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, boxHeight, 4, tft_color565(color, color, color) );
        delay(8); // TODO: use a timer
      }
      Out.drawScrollableRoundRect( 1, boxPosY, boxWidth, MacScrollView[card_index].blockHeight-2, 4, MacScrollView[card_index].borderColor );
      giveMuxSemaphore();
    }

    static void percentBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t percent, uint16_t barcolor, uint16_t bgcolor, uint16_t bordercolor = BLE_DARKGREY) {
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
      uint8_t yoffsetpercent = percent / ratio;
      uint8_t boxh = (yoffsetpercent * h) / ratio ;
      tft.fillRect(x, y, w, boxh, barcolor);

      uint8_t xoffsetpercent = percent % (int)ratio;
      if (xoffsetpercent == 0) return;
      uint8_t linew = (xoffsetpercent * w) / ratio;
      tft.drawFastHLine(x, y + boxh, linew, barcolor);
    }


    static void lineTo( uint16_t x, uint16_t y, uint16_t color) {
      tft.drawLine( prevx, prevy, x, y, color );
      prevx = x;
      prevy = y;
    }


    static void drawBluetoothLogo( uint16_t x, uint16_t y, uint8_t height = 10, uint16_t color = BLE_WHITE, uint16_t bgcolor = BLUETOOTH_COLOR ) {
      if( height<10) height=10; // low cap
      if( height%2!=0) height++; // lame centering

      tft.fillRoundRect( x+height/4, y+height*0.05, height/2, height-height*0.1, height/4, bgcolor );

      x += height*.1;
      y += height*.1;
      height *= .8;

      float y1 = height * 0.05;
      float y2 = height * 0.25;

      prevx = x + y2;
      prevy = y + y2;

      lineTo( x + height - y2, y + height - y2, color );
      lineTo( x + height/2,    y + height - y1, color );
      lineTo( x + height/2,    y + y1,          color );
      lineTo( x + height - y2, y + y2,          color );
      lineTo( x + y2,          y + height - y2, color );

    }


    // draws a RSSI Bar for the BLECard
    static void drawRSSIBar(int16_t x, int16_t y, int16_t rssi, uint16_t bgcolor, float size=1.0) {
      uint16_t barColors[4] = { bgcolor, bgcolor, bgcolor, bgcolor };
      switch(rssi%6) {
      case 5:
          barColors[0] = BLE_GREEN;
          barColors[1] = BLE_GREEN;
          barColors[2] = BLE_GREEN;
          barColors[3] = BLE_GREEN;
        break;
        case 4:
          barColors[0] = BLE_GREEN;
          barColors[1] = BLE_GREEN;
          barColors[2] = BLE_GREEN;
        break;
        case 3:
          barColors[0] = BLE_YELLOW;
          barColors[1] = BLE_YELLOW;
          barColors[2] = BLE_YELLOW;
        break;
        case 2:
          barColors[0] = BLE_YELLOW;
          barColors[1] = BLE_YELLOW;
        break;
        case 1:
          barColors[0] = BLE_RED;
        break;
        default:
        case 0:
          barColors[0] = BLE_RED; // want: RAINBOW
        break;
      }
      tft.fillRect(x,          y + 4*size, 2*size, 4*size, barColors[0]);
      tft.fillRect(x + 3*size, y + 3*size, 2*size, 5*size, barColors[1]);
      tft.fillRect(x + 6*size, y + 2*size, 2*size, 6*size, barColors[2]);
      tft.fillRect(x + 9*size, y + 1*size, 2*size, 7*size, barColors[3]);
    }

  private:

    static void animClearRect( int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color ) {
      while( w > 0 && h > 0 ) {
        tft.drawRect( x, y, w, h, color );
        x++;
        y++;
        w-=2;
        h-=2;
        delay(10);
      }
    }
    static void animClear( int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t bgcolor, uint16_t highlightcolor ) {
      animClearRect( x, y, w, h, highlightcolor );
      animClearRect( x, y, w, h, bgcolor );
    }

    void setIconBar() {

      // configure widgets
      BLERssiWidget.type        = ICON_WIDGET_RSSI;
      BLERssiWidget.color       = BLECardTheme.textColor;
      BLERssiWidget.cb          = &BLERssiIconUpdateCB;

      TextCountersWidget.type   = ICON_WIDGET_TEXT;
      TextCountersWidget.color  = BLE_GREENYELLOW;
      TextCountersWidget.cb     = &TextCountersIconUpdateCB;

      TextCountersIcon.posX     = heapStrX;
      TextCountersIcon.posY     = heapStrY;
      TextCountersIcon.bgcolor  = HEADER_BGCOLOR;

      rssiPointer         = &drawRSSIBar;
      textAlignPointer    = &alignTextAt;
      fillCirclePointer   = &tft_fillCircle;
      drawCirclePointer   = &tft_drawCircle;
      fillRectPointer     = &tft_fillRect;
      fillTrianglePointer = &tft_fillTriangle;

      BLECollectorIconBar.pushIcon( &TimeIcon );
      BLECollectorIconBar.pushIcon( &GPSIcon );
      BLECollectorIconBar.pushIcon( &VendorFilterIcon );
      BLECollectorIconBar.pushIcon( &BLEActivityIcon );
      BLECollectorIconBar.pushIcon( &BLERoleIcon );
      BLECollectorIconBar.pushIcon( &DBIcon );
      BLECollectorIconBar.pushIcon( &BLERssiIcon );
      BLECollectorIconBar.pushIcon( &TextCountersIcon );
      BLECollectorIconBar.setMargin( BLECollectorIconBarM );
      BLECollectorIconBar.init();
      BLECollectorIconBarX = Out.width - BLECollectorIconBar.width;

    }


    static void alignTextAt(const char* text, uint16_t x, uint16_t y, uint16_t color = BLE_YELLOW, uint16_t bgcolor = BLE_TRANSPARENT, uint8_t textAlign = ALIGN_FREE) {
      if( isEmpty( text ) ) return;
      if( bgcolor != BLE_TRANSPARENT ) {
        tft.setTextColor( color, bgcolor );
      } else {
        tft.setTextColor( color, color ); // force transparency
      }
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
      tft.drawString( text, tft.getCursorX(), tft.getCursorY() );
    }

    static DisplayMode getDisplayMode() {
      if( tft.width() > tft.height() ) {
        return TFT_LANDSCAPE;
      }
      if( tft.width() < tft.height() ) {
        return TFT_PORTRAIT;
      }
      return TFT_SQUARE;
    }

    // landscape / portrait theme switcher
    static void setUISizePos() {
      // TODO: dynamize these
      iconBleX = 104;
      iconBleY = 7;
      iconDbX = 116;
      iconDbY = 7;
      iconR = 4; // BLE icon radius
      displayMode = getDisplayMode();

      switch( displayMode ) {
        case TFT_LANDSCAPE:
          log_w("Using UI in landscape mode (w:%d, h:%d)", Out.width, Out.height);
          sprintf(UpTimeStringTplTpl, "%s", "Up:%9s");
          sprintf(seenDevicesCountSpacer, "%s", "   "); // Seen
          sprintf(scansCountSpacer, "%s", "  "); // Scans
          iconAppX            = 124;
          headerHeight        = 36; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          footerHeight        = 12; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          scrollHeight        = ( Out.height - ( headerHeight + footerHeight ));
          leftMargin          = 2;
          footerBottomPosY    = Out.height;
          headerStatsX        = Out.width - 80;
          graphLineWidth      = 60;//heapMapBuffLen - 1;
          heapMapBuffLen     = graphLineWidth+1; // add 1 for scroll
          graphLineHeight     = 29;
          graphX              = Out.width - (150);
          graphY              = 0; // footerBottomPosY - 37;// 283
          percentBoxX         = (graphX - 12); // percentbox is 10px wide + 2px margin and 2px border
          percentBoxY         = graphLineHeight+2;
          percentBoxSize      = 8;
          headerStatsIconsX   = Out.width - (80 + 6);
          headerStatsIconsY   = 4;
          headerLineHeight    = 16;
          progressBarY        = 32;
          hhmmPosX            = 200;
          hhmmPosY            = footerBottomPosY - 9;
          uptimePosX          = Out.width-80;
          uptimePosY          = headerStatsIconsY + headerLineHeight;
          uptimeAlign         = ALIGN_RIGHT;
          copyleftPosX        = 250;
          copyleftPosY        = footerBottomPosY - 9;
          cdevcPosX           = Out.width-80;
          cdevcPosY           = headerStatsIconsY + headerLineHeight;
          cdevcAlign          = ALIGN_RIGHT;
          sesscPosX           = Out.width-80;
          sesscPosY           = headerStatsIconsY + headerLineHeight;
          sesscAlign          = ALIGN_RIGHT;
          ndevcPosX           = Out.width-80;
          ndevcPosY           = headerStatsIconsY + headerLineHeight;
          ndevcAlign          = ALIGN_RIGHT;
          macAddrColorsScaleX = 4;
          macAddrColorsScaleY = 2;
          macAddrColorsSizeX  = 8 * macAddrColorsScaleX;
          macAddrColorsSizeY  = 8 * macAddrColorsScaleY;
          macAddrColorsSize   = macAddrColorsSizeX * macAddrColorsSizeY;
          macAddrColorsPosX   = Out.width - ( macAddrColorsSizeX + 6 );
          showScanStats       = false;
          showHeap            = false;
          showEntries         = true;
          showCdevc           = false;
          showSessc           = false;
          showNdevc           = false;
          showUptime          = false;
          heapStrX            = Out.width-80;
          heapStrY            = headerStatsIconsY + headerLineHeight;
          heapAlign           = ALIGN_RIGHT;
          entriesStrX         = Out.width-80;
          entriesStrY         = headerStatsIconsY + headerLineHeight;
          entriesAlign        = ALIGN_RIGHT;
          BLECollectorIconBarM= 4;
          BLECollectorIconBarX= Out.width - 74;
          BLECollectorIconBarY= 3;
          hallOfMacPosX       = 0;
          hallOfMacPosY       = footerBottomPosY - 10;
          hallOfMacHmargin    = 3;
          hallOfMacVmargin    = 1;
          hallOfMacItemWidth  = 16 + hallOfMacHmargin*2;
          hallOfMacItemHeight = 8 + hallOfMacVmargin*2;
          hallofMacCols       = 8;
          hallofMacRows       = 1;
        break;
        case TFT_PORTRAIT:
          log_w("Using UI in portrait mode");
          sprintf(UpTimeStringTplTpl, "%s", "Up:%9s");
          sprintf(seenDevicesCountSpacer, "%s", "   "); // Seen
          sprintf(scansCountSpacer, "%s", "  "); // Scans
          iconAppX            = 124;
          headerHeight        = 35; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          footerHeight        = 45; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          scrollHeight        = ( Out.height - ( headerHeight + footerHeight ));
          leftMargin          = 2;
          footerBottomPosY    = Out.height;
          headerStatsX        = Out.width-76;
          graphLineWidth      = 70;//heapMapBuffLen - 1;
          heapMapBuffLen     = graphLineWidth+1; // add 1 for scroll
          graphLineHeight     = 35;
          graphX              = Out.width - graphLineWidth - 2;
          graphY              = footerBottomPosY - 37;// 283
          percentBoxX         = (graphX - 14); // percentbox is 10px wide + 2px margin and 2px border
          percentBoxY         = footerBottomPosY;
          percentBoxSize      = 10;
          headerStatsIconsX   = 156;
          headerStatsIconsY   = 4;
          headerLineHeight    = 14;
          progressBarY        = 30;
          hhmmPosX            = 99;
          hhmmPosY            = footerBottomPosY - 28;
          uptimePosX          = Out.width-80;
          uptimePosY          = headerStatsIconsY + headerLineHeight;
          uptimeAlign         = ALIGN_RIGHT;
          copyleftPosX        = 79;
          copyleftPosY        = footerBottomPosY - 12;
          cdevcPosX           = Out.width-80;
          cdevcPosY           = headerStatsIconsY + headerLineHeight;
          cdevcAlign          = ALIGN_RIGHT;
          sesscPosX           = Out.width-80;
          sesscPosY           = headerStatsIconsY + headerLineHeight;
          sesscAlign          = ALIGN_RIGHT;
          ndevcPosX           = Out.width-80;
          ndevcPosY           = headerStatsIconsY + headerLineHeight;
          ndevcAlign          = ALIGN_RIGHT;
          macAddrColorsScaleX = 4;
          macAddrColorsScaleY = 2;
          macAddrColorsSizeX  = 8 * macAddrColorsScaleX;
          macAddrColorsSizeY  = 8 * macAddrColorsScaleY;
          macAddrColorsPosX   = Out.width - ( macAddrColorsSizeX + 6 );
          showScanStats       = false;
          showHeap            = false;
          showEntries         = true;
          showCdevc           = false;
          showSessc           = false;
          showNdevc           = false;
          showUptime          = false;
          heapStrX            = Out.width-80;
          heapStrY            = headerStatsIconsY + headerLineHeight;
          heapAlign           = ALIGN_RIGHT;
          entriesStrX         = Out.width-80;
          entriesStrY         = headerStatsIconsY + headerLineHeight;
          entriesAlign        = ALIGN_RIGHT;
          BLECollectorIconBarM= 4;
          BLECollectorIconBarX= Out.width - 84;
          BLECollectorIconBarY= 3;
          hallOfMacPosX       = 0;
          hallOfMacPosY       = footerBottomPosY - 36;
          hallOfMacHmargin    = 4;
          hallOfMacVmargin    = 2;
          hallOfMacItemWidth  = 16 + hallOfMacHmargin*2;
          hallOfMacItemHeight = 8 + hallOfMacVmargin*2;
          hallofMacCols       = 3;
          hallofMacRows       = 3;
        break;
        case TFT_SQUARE:
        default:
          TextCountersIcon.srcStatus = NULL; // don't decorate text counters = save 10px
          log_w("Using UI in square/squeezed mode (w:%d, h:%d)", Out.width, Out.height);
          sprintf(UpTimeStringTplTpl, "%s", "Up:%9s");
          sprintf(seenDevicesCountSpacer, "%s", "   "); // Seen
          sprintf(scansCountSpacer, "%s", "  "); // Scans
          iconAppX            = 120;
          headerHeight        = 37; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          footerHeight        = 11; // Important: resulting scrollHeight must be a multiple of font height, default font height is 8px
          scrollHeight        = ( Out.height - ( headerHeight + footerHeight ));
          leftMargin          = 2;
          footerBottomPosY    = Out.height;
          headerStatsX        = Out.width - 80;
          graphLineWidth      = 73;
          heapMapBuffLen     = graphLineWidth+1; // add 1 for scroll
          graphLineHeight     = 18;
          graphX              = Out.width - (graphLineWidth);
          graphY              = 13; // footerBottomPosY - 37;// 283
          percentBoxX         = (graphX - 12); // percentbox is 10px wide + 2px margin and 2px border
          percentBoxY         = graphY+graphLineHeight+2;
          percentBoxSize      = 8;
          headerStatsIconsX   = Out.width - (80 + 6);
          headerStatsIconsY   = 4;
          headerLineHeight    = 16;
          progressBarY        = 33;
          hhmmPosX            = 63;
          hhmmPosY            = footerBottomPosY - 9;
          uptimePosX          = headerStatsX;
          uptimePosY          = footerBottomPosY - 9;
          uptimeAlign         = ALIGN_RIGHT;
          copyleftPosX        = 97;
          copyleftPosY        = footerBottomPosY - 9;
          cdevcPosX           = headerStatsX;
          cdevcPosY           = footerBottomPosY - 9;
          cdevcAlign          = ALIGN_RIGHT;
          sesscPosX           = headerStatsX;
          sesscPosY           = footerBottomPosY - 9;
          sesscAlign          = ALIGN_RIGHT;
          ndevcPosX           = headerStatsX;
          ndevcPosY           = footerBottomPosY - 9;
          ndevcAlign          = ALIGN_RIGHT;
          macAddrColorsScaleX = 4;
          macAddrColorsScaleY = 2;
          macAddrColorsSizeX  = 8 * macAddrColorsScaleX;
          macAddrColorsSizeY  = 8 * macAddrColorsScaleY;
          macAddrColorsSize   = macAddrColorsSizeX * macAddrColorsSizeY;
          macAddrColorsPosX   = Out.width - ( macAddrColorsSizeX + 6 );
          showScanStats       = false;
          showHeap            = false;
          showEntries         = true;
          showCdevc           = false;
          showSessc           = false;
          showNdevc           = false;
          showUptime          = false;
          heapStrX            = headerStatsX;
          heapStrY            = footerBottomPosY - 9;
          heapAlign           = ALIGN_RIGHT;
          entriesStrX         = headerStatsX;
          entriesStrY         = footerBottomPosY - 9;
          entriesAlign        = ALIGN_RIGHT;
          BLECollectorIconBarM= 2;
          BLECollectorIconBarX= Out.width - 72;
          BLECollectorIconBarY= 0;
          hallOfMacPosX       = 0;
          hallOfMacPosY       = footerBottomPosY - 10;
          hallOfMacHmargin    = 2;
          hallOfMacVmargin    = 1;
          hallOfMacItemWidth  = 16 + hallOfMacHmargin*2;
          hallOfMacItemHeight = 8 + hallOfMacVmargin*2;
          hallofMacCols       = 3;
          hallofMacRows       = 1;
        break;
      }

      hallOfMacSize       = hallofMacCols*hallofMacRows;
      iconAppY = (headerHeight-4)/2 - Icon_tbz_src->height/2;
      // init some heap graph variables
      toleranceheap = min_free_heap + heap_tolerance;
      baseCoordY = graphLineHeight-2; // set Y axis to 2px-bottom of the graph
      dcpmFirstY = baseCoordY;
      dcpmppFirstY = baseCoordY;

      log_w("Allocating heapgraph buffers (%d and %d bytes)", heapMapBuffLen*sizeof( uint16_t ),  heapMapBuffLen*sizeof( uint32_t ));

      devCountPerMinutePerPeriod = (uint16_t*)calloc( heapMapBuffLen, sizeof( uint16_t ) );
      if( devCountPerMinutePerPeriod != NULL ) {
        log_d("'devCountPerMinutePerPeriod' allocation successful");
      } else {
        log_e("'devCountPerMinutePerPeriod' allocation of %d bytes failed!", heapMapBuffLen*sizeof( uint32_t ) );
      }

      heapmap = (uint32_t*)calloc( heapMapBuffLen, sizeof( uint32_t ) );
      if( heapmap != NULL ) {
        log_d("'heapmap' allocation successful");
      } else {
        log_e("'heapmap' allocation of %d bytes failed!", heapMapBuffLen*sizeof( uint32_t ) );
      }

    }

};


UIUtils UI;
