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

#define SPACE " "
static bool isScrolling = false;
static bool isInScroll()
{
  return isScrolling;
}

class ScrollableOutput
{

  enum ScrollType
  {
    SCROLL_HARDWARE,
    SCROLL_SOFTWARE
  };

  public:
    uint16_t height;// = scrollpanel_height();
    uint16_t width;//  = scrollpanel_width();
    // scroll control variables
    uint16_t scrollTopFixedArea = 0;
    uint16_t scrollBottomFixedArea = 0;
    uint16_t yRef = scrollTopFixedArea;
    uint16_t yArea = height - scrollTopFixedArea - scrollBottomFixedArea;
    int16_t  x1_tmp, y1_tmp;
    uint16_t w_tmp, h_tmp;
    int scrollPosY = -1;
    int scrollPosX = -1;
    bool serialEcho = true;
    //uint16_t BgColor;
    RGBColor BGColorStart;
    RGBColor BGColorEnd;
    ScrollType scrollType = SCROLL_HARDWARE;

    void init()
    {
      height = scrollpanel_height();
      width  = scrollpanel_width();
    }

    int println()
    {
      return println(" ");
    }

    int println(const char* str)
    {
      char output[256] = {'\0'};
      sprintf(output, "%s\n", str);
      return print(output);
    }

    int print(const char* str)
    {
      if(strcmp(str, " \n")!=0 && serialEcho) {
        Serial.print( str );
      }
      return scroll(str);
    }

    void setupScrollArea(uint16_t TFA, uint16_t BFA, RGBColor colorstart, RGBColor colorend, bool clear = false)
    {
      BGColorStart = colorstart;
      BGColorEnd = colorend;
      tft.setCursor(0, TFA);
      uint16_t VSA = height-(TFA+BFA);

      scrollTopFixedArea    = TFA;
      scrollBottomFixedArea = BFA;

      yRef       = scrollTopFixedArea;
      scrollPosY = scrollTopFixedArea;
      yArea = height - scrollTopFixedArea - scrollBottomFixedArea;

      tft_setupHScrollArea(0, 0, 0); // reset ?
      tft_setupHScrollArea(TFA, VSA, BFA);
      s_x_tmp = 0;
      s_y_tmp = scrollTopFixedArea;
      s_w_tmp = width;
      s_h_tmp = yArea;

      log_d("*** NEW Scroll Setup: Top=%d Bottom=%d YArea=%d", TFA, BFA, yArea);
      if (clear) {
        tft_fillGradientHRect( 0, TFA, width/2, yArea, BGColorStart, BGColorEnd );
        tft_fillGradientHRect( width/2, TFA, width/2, yArea, BGColorEnd, BGColorStart );
      }
    }

    void scrollNextPage()
    {
      tft_getTextBounds("O", scrollPosX, scrollPosY, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
      int linesInPage = (yArea-(scrollPosY-scrollTopFixedArea)) / h_tmp;
      for(int i=0; i<linesInPage; i++) {
        println();
      }
    }

    int translate(int y, int distance=0)
    {
      return ( ( yArea + ( ( y - scrollTopFixedArea) + distance) ) % yArea ) + scrollTopFixedArea;
    }

    // draw rounded corners boxes inside the scroll view, with scroll limit overlap support
    void drawScrollableRoundRect(uint16_t x, uint16_t y, uint16_t _width, uint16_t _height, uint16_t radius, uint16_t bordercolor, bool fill = false )
    {
      int yStart = translate(y, 0); // get the scrollview-translated y position
      if ( yStart >= scrollTopFixedArea && (yStart+_height)<(height-scrollBottomFixedArea) ) {
        // no scroll loop point overlap, just render the translated box using the native method
        log_v("Rendering native x:%d, y:%d, width:%d, height:%d, radius:%d", x, y, _width, _height, radius);
        if( fill ) {
          tft.fillRoundRect(x, yStart, _width, _height, radius, bordercolor);
        } else {
          tft.drawRoundRect(x, yStart, _width, _height, radius, bordercolor);
        }
      } else {
        // box overlaps the scroll limit, split it in two chunks!
        int yEnd = translate(y, _height);
        int lowerBlockHeight = yEnd - scrollTopFixedArea;
        int upperBlockHeight = (scrollTopFixedArea + yArea) - yStart;
        int leftVlinePosX    = x;
        int rightVlinePosX   = x+_width-1;
        int leftHLinePosX    = leftVlinePosX + radius;
        int rightHlinePosX   = rightVlinePosX - radius;
        int hLineWidth       = _width - 2 * radius;
        log_v("Rendering split x:%d, y:%d, width:%d, height:%d, radius:%d", x, y, _width, _height, radius);
        log_v("                yStart:%d, yEnd:%d, upperBlockHeight:%d, lowerBlockHeight:%d", yStart, yEnd, upperBlockHeight, lowerBlockHeight);

        tft.drawFastHLine(leftHLinePosX, yStart, hLineWidth, bordercolor); // upper hline
        tft.drawFastHLine(leftHLinePosX, yEnd-1, hLineWidth, bordercolor); // lower hline
        if (upperBlockHeight > radius) {
          // don't bother rendering the corners if there isn't enough height left
          tft.drawFastVLine(leftVlinePosX,  yStart + radius, upperBlockHeight - radius, bordercolor); // upper left vline
          tft.drawFastVLine(rightVlinePosX, yStart + radius, upperBlockHeight - radius, bordercolor); // upper right vline
        }
        if (lowerBlockHeight > radius) {
          // don't bother rendering the corners if there isn't enough height left
          tft.drawFastVLine(leftVlinePosX,  yEnd - lowerBlockHeight, lowerBlockHeight - radius, bordercolor); // lower left vline
          tft.drawFastVLine(rightVlinePosX, yEnd - lowerBlockHeight, lowerBlockHeight - radius, bordercolor); // lower right vline
        }
        tft.startWrite();
        tft.drawCircleHelper(leftHLinePosX,  yStart + radius, radius, 1, bordercolor); // upper left
        tft.drawCircleHelper(rightHlinePosX, yStart + radius, radius, 2, bordercolor); // upper right
        tft.drawCircleHelper(rightHlinePosX, yEnd - radius-1, radius, 4, bordercolor); // lower right
        tft.drawCircleHelper(leftHLinePosX,  yEnd - radius-1, radius, 8, bordercolor); // lower left
        tft.endWrite();
      }
    }

  private:

    int scroll(const char* str)
    {
      //tft.drawFastHLine( 0, scrollTopFixedArea, 8, BLE_RED );
      isScrolling = true;
      if (scrollPosY == -1) {
        scrollPosY = tft.getCursorY();
      }
      scrollPosX = tft.getCursorX();
      switch( scrollType ) {
        case SCROLL_HARDWARE:
          if (scrollPosY >= (height - scrollBottomFixedArea)) {
            scrollPosY = (scrollPosY % (height - scrollBottomFixedArea)) + scrollTopFixedArea;
          }
          tft_getTextBounds(str, scrollPosX, scrollPosY, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
        break;
        case SCROLL_SOFTWARE:
          tft_getTextBounds(str, scrollPosX, scrollPosY, &x1_tmp, &y1_tmp, &w_tmp, &h_tmp);
          scrollPosY = (scrollTopFixedArea + yArea) - h_tmp;
        break;
      }


      tft_fillGradientHRect( 0, scrollPosY, width/2, h_tmp, BGColorStart, BGColorEnd );
      tft_fillGradientHRect( width/2, scrollPosY, width/2, h_tmp, BGColorEnd, BGColorStart );
      tft.setCursor(scrollPosX, scrollPosY);
      scroll_slow(h_tmp, 3); // Scroll lines, 3ms per line
      //tft.print(str);
      if( strcmp(str, " \n")!=0 ) {
        tft.drawString( str, tft.getCursorX(), tft.getCursorY());
      }

      scrollPosY = tft.getCursorY() + h_tmp;
      tft.setCursor(0, scrollPosY);
      isScrolling = false;
      return h_tmp;
    }
    // change this function if your TFT does not handle hardware scrolling
    int scroll_slow(int lines, int wait)
    {
      int yTemp = yRef;
      //scrollPosY = -1;

      switch( scrollType ) {
        case SCROLL_HARDWARE:
          for (int i = 0; i < lines; i++) {
            yRef++;
            if (yRef == height - scrollBottomFixedArea) yRef = scrollTopFixedArea;
            tft_hScrollTo( yRef );
            delay(wait);
          }
        break;
        case SCROLL_SOFTWARE:
          tft_scrollTo(-lines);
          /*
          for (int i = 0; i < lines; i++) {
            if (yRef <= scrollTopFixedArea ) yRef = height - scrollBottomFixedArea;
            yRef--;
          }
          */
          yTemp = abs(lines);
        break;
      }
      return yTemp;
    }

};


ScrollableOutput Out;
