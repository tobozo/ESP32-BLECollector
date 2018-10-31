


void drawRSSI(int16_t x, int16_t y, int16_t rssi) {
  uint16_t barColors[4];
  if (rssi >= -30) {
    // -30 dBm and more Amazing    - Max achievable signal strength. The client can only be a few feet from the AP to achieve this. Not typical or desirable in the real world.  N/A
    barColors[0] = WROVER_GREEN;
    barColors[1] = WROVER_GREEN;
    barColors[2] = WROVER_GREEN;
    barColors[3] = WROVER_GREEN;
  } else if (rssi >= -67) {
    // between -67 dBm and 31 dBm  - Very Good   Minimum signal strength for applications that require very reliable, timely delivery of data packets.   VoIP/VoWiFi, streaming video
    barColors[0] = WROVER_GREEN;
    barColors[1] = WROVER_GREEN;
    barColors[2] = WROVER_GREEN;
    barColors[3] = WROVER_WHITE;
  } else if (rssi >= -70) {
    // between -70 dBm and -68 dBm - Okay  Minimum signal strength for reliable packet delivery.   Email, web
    barColors[0] = WROVER_YELLOW;
    barColors[1] = WROVER_YELLOW;
    barColors[2] = WROVER_YELLOW;
    barColors[3] = WROVER_WHITE;
  } else if (rssi >= -80) {
    // between -80 dBm and -71 dBm - Not Good  Minimum signal strength for basic connectivity. Packet delivery may be unreliable.  N/A
    barColors[0] = WROVER_YELLOW;
    barColors[1] = WROVER_YELLOW;
    barColors[2] = WROVER_WHITE;
    barColors[3] = WROVER_WHITE;
  } else if (rssi >= -90) {
    // between -90 dBm and -81 dBm - Unusable  Approaching or drowning in the noise floor. Any functionality is highly unlikely.
    barColors[0] = WROVER_RED;
    barColors[1] = WROVER_WHITE;
    barColors[2] = WROVER_WHITE;
    barColors[3] = WROVER_WHITE;
  }  else {
    // dude, this sucks
    barColors[0] = WROVER_RED;
    barColors[1] = WROVER_WHITE;
    barColors[2] = WROVER_WHITE;
    barColors[3] = WROVER_WHITE;
  }
  tft.fillRect(x,    y + 3, 2, 5, barColors[0]);
  tft.fillRect(x + 3,  y + 2, 2, 6, barColors[1]);
  tft.fillRect(x + 6,  y + 1, 2, 7, barColors[2]);
  tft.fillRect(x + 9,  y,   2, 8, barColors[3]);
}

enum TextDirections {
  ALIGN_FREE   = 0,
  ALIGN_LEFT   = 1,
  ALIGN_RIGHT  = 2,
  ALIGN_CENTER = 3,
};


void alignTextAt(const char* text, uint16_t x, uint16_t y, int16_t color = WROVER_YELLOW, int16_t bgcolor = WROVER_BLACK, byte textAlign = ALIGN_FREE) {
  tft.setTextColor(color);
  tft.getTextBounds(text, x, y, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
  switch (textAlign) {
    case ALIGN_FREE:
      tft.setCursor(x, y);
      tft.fillRect(x, y, w_tmp, h_tmp, bgcolor);
      break;
    case ALIGN_LEFT:
      tft.setCursor(0, y);
      tft.fillRect(0, y, w_tmp, h_tmp, bgcolor);
      break;
    case ALIGN_RIGHT:
      tft.setCursor(tft_width - w_tmp, y);
      tft.fillRect(tft_width - w_tmp, y, w_tmp, h_tmp, bgcolor);
      break;
    case ALIGN_CENTER:
      tft.setCursor(tft_width / 2 - w_tmp / 2, y);
      tft.fillRect(tft_width / 2 - w_tmp / 2, y, w_tmp, h_tmp, bgcolor);
      break;
  }
  tft.print(text);
}


void headerStats(String status = "") {
  int16_t posX = tft.getCursorX();
  int16_t posY = tft.getCursorX();
  String s_heap = " Heap: " + String(freeheap);
  String s_entries = " Entries: " + String(entries);
  alignTextAt(s_heap.c_str(), 128, 2, WROVER_RED, HEADER_BGCOLOR, ALIGN_RIGHT);
  if (status != "") {
    Serial.println(status);
    tft.fillRect(0, 16, tft_width, 8, HEADER_BGCOLOR); // clear whole area
    alignTextAt(status.c_str(), 0, 16, WROVER_YELLOW, HEADER_BGCOLOR, ALIGN_LEFT);
    tft.drawJpg( tbz_28x28_jpg, tbz_28x28_jpg_len, 134, 0, 28,  28);
  }
  alignTextAt(s_entries.c_str(), 128, 16, WROVER_RED, HEADER_BGCOLOR, ALIGN_RIGHT);
  tft.setCursor(posX, posY);
}


void footerStats() {
  int16_t posX = tft.getCursorX();
  int16_t posY = tft.getCursorX();
  alignTextAt(String(timeString).c_str(), 128, 300, WROVER_YELLOW, FOOTER_BGCOLOR, ALIGN_CENTER);
  alignTextAt(String(UpTimeString).c_str(), 128, 310, WROVER_YELLOW, FOOTER_BGCOLOR, ALIGN_CENTER);
  String sessDevicesCountStr = "Total: " + String(sessDevicesCount) + " ";
  String devicesCountStr = "Last:  " + String(devicesCount) + " ";
  String newDevicesCountStr = "New:   " + String(newDevicesCount) + " ";
  alignTextAt(devicesCountStr.c_str(), 0, 290, WROVER_ORANGE, FOOTER_BGCOLOR, ALIGN_LEFT);
  alignTextAt(sessDevicesCountStr.c_str(), 0, 300, WROVER_ORANGE, FOOTER_BGCOLOR, ALIGN_LEFT);
  alignTextAt(newDevicesCountStr.c_str(), 0, 310, WROVER_ORANGE, FOOTER_BGCOLOR, ALIGN_LEFT);
  //heapGraph();
  tft.setCursor(posX, posY);
}


void initUI(bool clearScreen = true) {
  tft.begin();
  tft.setRotation( 0 ); // required to get smooth scrolling
  tft.setTextColor(WROVER_YELLOW);
  if (clearScreen) {
    tft.fillScreen(WROVER_BLACK);
    tft.fillRect(0, HEADER_HEIGHT, tft_width, SCROLL_HEIGHT, BLECARD_BGCOLOR);
    // clear heap map
    for (uint16_t i = 0; i < HEAPMAP_BUFFLEN; i++) heapmap[i] = 0;
  }
  tft.fillRect(0, 0, tft_width, HEADER_HEIGHT, HEADER_BGCOLOR);
  tft.fillRect(0, tft_height - FOOTER_HEIGHT, tft_width, FOOTER_HEIGHT, FOOTER_BGCOLOR);
  tft.fillRect(0, PROGRESSBAR_Y, tft_width, 2, WROVER_ORANGE);
  //tft.getTextBounds(welcomeMSG, 0, 1, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
  alignTextAt(welcomeMSG, 0, 1, WROVER_YELLOW, HEADER_BGCOLOR, ALIGN_LEFT);
  headerStats("Init UI");
  footerStats();
  tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_ORANGE);
  setupScrollArea(HEADER_HEIGHT, FOOTER_HEIGHT);
}


/* task core 0 */
void heapGraph(void * parameter) {
  uint32_t lastfreeheap;
  uint8_t i = 0;
  while (1) {
    // only redraw if the heap changed
    if(lastfreeheap!=freeheap) {
      heapmap[++heapindex] = freeheap;
      heapindex = heapindex % HEAPMAP_BUFFLEN;
      lastfreeheap = freeheap;
    } else {
      vTaskDelay(300);
      continue;
    }
    uint16_t GRAPH_COLOR = WROVER_WHITE;
    uint32_t graphMin = min_free_heap;
    uint32_t graphMax = graphMin;
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
    // draw graph
    for (i = 0; i < GRAPH_LINE_WIDTH; i++) {
      int thisindex = int(heapindex - GRAPH_LINE_WIDTH + i + HEAPMAP_BUFFLEN) % HEAPMAP_BUFFLEN;
      uint32_t heapval = heapmap[thisindex];
      if( heapval > min_free_heap + heap_tolerance ) {
        GRAPH_COLOR = WROVER_GREEN;
      } else {
        if( heapval > min_free_heap ) GRAPH_COLOR = WROVER_DARKGREEN;
        else GRAPH_COLOR = WROVER_ORANGE;
      }
      tft.drawLine( GRAPH_X + i, GRAPH_Y, GRAPH_X + i, GRAPH_Y + GRAPH_LINE_HEIGHT, WROVER_BLACK );
      if ( heapval > 0 ) {
        uint32_t lineheight = map(heapval, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
        tft.drawLine( GRAPH_X + i, GRAPH_Y + GRAPH_LINE_HEIGHT, GRAPH_X + i, GRAPH_Y + GRAPH_LINE_HEIGHT - lineheight, GRAPH_COLOR );
      }
    }
    if(graphMin!=graphMax) {
      uint32_t toleranceline = map(min_free_heap + heap_tolerance, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
      tft.drawFastHLine( GRAPH_X, GRAPH_Y + GRAPH_LINE_HEIGHT - toleranceline, GRAPH_LINE_WIDTH, WROVER_DARKGREY );
      uint32_t minline = map(min_free_heap, graphMin, graphMax, 0, GRAPH_LINE_HEIGHT);
      tft.drawFastHLine( GRAPH_X, GRAPH_Y + GRAPH_LINE_HEIGHT - minline, GRAPH_LINE_WIDTH, WROVER_RED );
    }
  }
}


/* task core 0 */
void blinkBlueIcon( void * parameter ) {
  unsigned long now = millis();
  unsigned long scanTime = SCAN_TIME * 1000;
  unsigned long then = now + scanTime;
  unsigned long lastblink = millis();
  unsigned long lastprogress = millis();
  bool toggler = true;
  while (now < then) {
    now = millis();
    if (lastblink + random(333, 666) < now) {
      toggler = !toggler;
      if (toggler) {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_BLUE);
      } else {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R - 1, HEADER_BGCOLOR);
      }
      lastblink = now;
    }
    if (lastprogress + 1000 < now) {
      unsigned long remaining = then - now;
      int percent = 100 - ( ( remaining * 100 ) / scanTime );
      tft.fillRect(0, PROGRESSBAR_Y, (tft_width * percent) / 100, 2, WROVER_BLUE);
      lastprogress = now;
    }
    vTaskDelay(30);
  }
  // clear progress bar
  tft.fillRect(0, PROGRESSBAR_Y, tft_width, 2, WROVER_ORANGE);
  // clear blue pin
  tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_ORANGE);
  //Serial.println("Ending blinkBlueIcon");
  vTaskDelete( NULL );
}
