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


static bool isScrolling = false;
static uint16_t BGCOLOR;
volatile xSemaphoreHandle displayWrite = NULL; // this is needed to prevent rendering collisions 
                                     // between scrollpanel and heap graph
static xSemaphoreHandle mux = NULL;

class ScrollableOutput {
  public:
    uint16_t height = tft.height();//ILI9341_HEIGHT (=320)
    uint16_t width  = tft.width();//ILI9341_WIDTH (=240)
    // scroll control variables
    uint16_t scrollTopFixedArea = 0;
    uint16_t scrollBottomFixedArea = 0;
    uint16_t yStart = scrollTopFixedArea;
    uint16_t yArea = height - scrollTopFixedArea - scrollBottomFixedArea;
    int16_t  x1_tmp, y1_tmp;
    uint16_t w_tmp, h_tmp;
    int scrollPosY = -1;
    int scrollPosX = -1;
    /*
    int printf(char* fmt ...) { // printf for the poor :-)
      char buf[1024]; // resulting string limited to 1024 chars
      va_list args;
      va_start (args, fmt );
      vsnprintf(buf, 1024, fmt, args);
      va_end (args);
      return print(buf);
    }*/
    int println() {
      return println(" ");
    }
    int println(const char* str) {
      char output[256] = {'\0'};
      sprintf(output, "%s\n", str);
      return print(output);
    }
    /*
    int println(String str = " ") { 
      // force a space to increment scroll on the display
      return print(str + "\n");
    }*/
    int print(const char* str) {
      if(strcmp(str, " \n")!=0) {
        Serial.print( str );
      }
      return scroll(str);
    }
    /*
    int print(String str) {
      if(str!=" \n") {
        // avoid unnecessary scrolling in the serial console
        Serial.print( str );
      }
      return scroll(str);
    }*/
    void setupScrollArea(uint16_t TFA, uint16_t BFA, bool clear = false) {
      tft.setCursor(0, TFA);
      tft.setupScrollArea(TFA, BFA); // driver needs patching for that, see https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
      scrollPosY = TFA;
      scrollTopFixedArea = TFA;
      scrollBottomFixedArea = BFA;
      yStart = scrollTopFixedArea;
      yArea = height - scrollTopFixedArea - scrollBottomFixedArea;
      //Serial.printf("*** NEW Scroll Setup: Top=%d Bottom=%d YArea=%d\n", TFA, BFA, yArea);
      if (clear) {
        tft.fillRect(0, TFA, width, yArea, WROVER_BLACK);
      }
    }
  private:
    /*
    int scroll(String str) {
      return scroll( str.c_str() );
    }*/
    int scroll(const char* str) {
      isScrolling = true;
      if (scrollPosY == -1) {
        scrollPosY = tft.getCursorY();
      }
      scrollPosX = tft.getCursorX();
      if (scrollPosY >= (height - scrollBottomFixedArea)) {
        scrollPosY = (scrollPosY % (height - scrollBottomFixedArea)) + scrollTopFixedArea;
      }
      tft.getTextBounds(str, scrollPosX, scrollPosY, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
  
      if (scrollPosX == 0) {
        tft.fillRect(0, scrollPosY, width, h_tmp, BGCOLOR);
      } else { // fill the horizontal gap
        tft.fillRect(0, scrollPosY, w_tmp, h_tmp, BGCOLOR);
      }
      tft.setCursor(scrollPosX, scrollPosY);
      scroll_slow(h_tmp, 5); // Scroll lines, 5ms per line
      tft.print(str);
      scrollPosY = tft.getCursorY();
      isScrolling = false;
      return h_tmp;
    }
    /* change this function if your TFT does not handle hardware scrolling */
    int scroll_slow(int lines, int wait) {
      int yTemp = yStart;
      scrollPosY = -1;
      for (int i = 0; i < lines; i++) {
        yStart++;
        if (yStart == height - scrollBottomFixedArea) yStart = scrollTopFixedArea;
        tft.scrollTo(yStart);
        delay(wait);
      }
      return  yTemp;
    }

};


ScrollableOutput Out;
