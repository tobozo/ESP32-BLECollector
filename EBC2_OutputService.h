
void drawRSSI(int16_t x, int16_t y, int16_t rssi);

void setupScrollArea(uint16_t TFA, uint16_t BFA, bool clear = false) {
  tft.setCursor(0, TFA);
  tft.setupScrollArea(TFA, BFA);
  scrollPosY = TFA;
  scrollTopFixedArea = TFA;
  scrollBottomFixedArea = BFA;
  yStart = scrollTopFixedArea;
  yArea = tft_height - scrollTopFixedArea - scrollBottomFixedArea;
  //Serial.printf("*** NEW Scroll Setup: Top=%d Bottom=%d YArea=%d\n", TFA, BFA, yArea);
  if (clear) {
    tft.fillRect(0, TFA, tft_width, yArea, WROVER_BLACK);
  }
}


int scroll_slow(int lines, int wait) {
  int yTemp = yStart;
  scrollPosY = -1;
  for (int i = 0; i < lines; i++) {
    yStart++;
    if (yStart == tft_height - scrollBottomFixedArea) yStart = scrollTopFixedArea;
    tft.scrollTo(yStart);
    delay(wait);
  }
  return  yTemp;
}


struct OutputService {
  int printf(char* fmt ...) {
    char buf[1024]; // resulting string limited to 1024 chars
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 1024, fmt, args);
    va_end (args);
    return print(buf);
  }
  int println(String str = SPACE) {
    return print(str + "\n");
  }
  int print(String str) {
    Serial.print( str );
    return scroll(str);
  }
  int printBLECard(BlueToothDevice &BLEDev) {
    uint16_t randomcolor = tft.color565(random(128, 255), random(128, 255), random(128, 255));
    uint16_t pos = 0;
    uint16_t hop;
    tft.setTextColor(randomcolor);
    //pos+=println("/---------------------------------------");
    hop = println(SPACE);
    pos += hop;

    if (BLEDev.address != "" && BLEDev.rssi != "") {
      lastPrintedMac[lastPrintedMacIndex++%BLECARD_MAC_CACHE_SIZE] = BLEDev.address;
      //lastPrintedMac = BLEDev.address;
      uint8_t len = MAX_ROW_LEN - (BLEDev.address.length() + BLEDev.rssi.length());
      hop = println( "  " + BLEDev.address + String(std::string(len, ' ').c_str()) + BLEDev.rssi + " dBm" );
      pos += hop;
      drawRSSI(tft_width - 18, scrollPosY - hop, atoi( BLEDev.rssi.c_str() ));
      if (BLEDev.in_db) {
        tft.drawJpg( update_jpeg, update_jpeg_len, 138, scrollPosY - hop, 8,  8);
      } else {
        tft.drawJpg( insert_jpeg, insert_jpeg_len, 138, scrollPosY - hop, 8,  8);
      }
      if (BLEDev.uuid != "") {
        // Service ID Icon
        tft.drawJpg( service_jpeg, service_jpeg_len, 128, scrollPosY - hop, 8,  8);
      }
    }

    if (BLEDev.ouiname != "") {
      pos += println(SPACE);
      hop = println(SPACETABS + BLEDev.ouiname);
      pos += hop;
      tft.drawJpg( nic16_jpeg, nic16_jpeg_len, 10, scrollPosY - hop, 13, 8);
    }

    if (BLEDev.appearance != "") {
      pos += println(SPACE);
      hop = println("  Appearance: " + BLEDev.appearance);
      pos += hop;
    }

    if (BLEDev.name != "") {
      pos += println(SPACE);
      hop = println(SPACETABS + BLEDev.name);
      pos += hop;
      tft.drawJpg( name_jpeg, name_jpeg_len, 12, scrollPosY - hop, 7,  8);
    }

    if (BLEDev.vname != "") {
      pos += println(SPACE);
      hop = println(SPACETABS + BLEDev.vname);
      pos += hop;
      if (BLEDev.vname == "Apple, Inc.") {
        tft.drawJpg( apple16_jpeg, apple16_jpeg_len, 12, scrollPosY - hop, 8,  8);
      } else if (BLEDev.vname == "IBM Corp.") {
        tft.drawJpg( ibm8_jpg, ibm8_jpg_len, 10, scrollPosY - hop, 20,  8);
      } else if (BLEDev.vname == "Microsoft") {
        tft.drawJpg( crosoft_jpeg, crosoft_jpeg_len, 12, scrollPosY - hop, 8,  8);
      } else {
        tft.drawJpg( generic_jpeg, generic_jpeg_len, 12, scrollPosY - hop, 8,  8);
      }
    }

    hop = println(SPACE);
    pos += hop;

    if (scrollPosY - pos >= scrollTopFixedArea) {
      // no scroll loop point overlap, just render the box
      tft.drawRect(1, scrollPosY - pos + 1, tft_width - 2, pos - 2, BLEDev.deviceColor);
    } else {
      // last block overlaps scroll loop point and has been split
      int h1 = (scrollTopFixedArea - (scrollPosY - pos));
      int h2 = pos - h1;
      h1 -= 2; //margin
      h2 -= 2;; //margin
      int vpos1 = scrollPosY - pos + yArea + 1;
      int vpos2 = scrollPosY - 2;
      tft.drawFastHLine(1,           vpos1,    tft_width - 2, BLEDev.deviceColor); // upper hline
      tft.drawFastHLine(1,           vpos2,    tft_width - 2, BLEDev.deviceColor); // lower hline
      tft.drawFastVLine(1,           vpos1 + 1,  h1,          BLEDev.deviceColor); // upper left vline
      tft.drawFastVLine(tft_width - 2, vpos1 + 1,  h1,          BLEDev.deviceColor); // upper right vline
      tft.drawFastVLine(1,           vpos2 - h2, h2,          BLEDev.deviceColor); // lower left vline
      tft.drawFastVLine(tft_width - 2, vpos2 - h2, h2,          BLEDev.deviceColor); // lower right vline
    }
    return pos;
  }

  int scroll(String str) {
    if (scrollPosY == -1) {
      scrollPosY = tft.getCursorY();
    }
    scrollPosX = tft.getCursorX();
    if (scrollPosY >= (tft_height - scrollBottomFixedArea)) {
      scrollPosY = (scrollPosY % (tft_height - scrollBottomFixedArea)) + scrollTopFixedArea;
    }
    tft.getTextBounds(str, scrollPosX, scrollPosY, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);

    if (scrollPosX == 0) {
      tft.fillRect(0, scrollPosY, tft_width, h_tmp, BLECARD_BGCOLOR);
    } else {
      tft.fillRect(0, scrollPosY, w_tmp, h_tmp, BLECARD_BGCOLOR);
    }
    tft.setCursor(scrollPosX, scrollPosY);
    scroll_slow(h_tmp, 5); // Scroll lines, 5ms per line
    tft.print(str);

    scrollPosY = tft.getCursorY();

    return h_tmp;
  }
};


OutputService Out;
