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

#include <Update.h>
#define MENU_BIN F("/menu.bin")
#define SDU_PROGRESS_X 0
#define SDU_PROGRESS_Y 288
#define SDU_PROGRESS_W 150
#define SDU_PROGRESS_H 10
#define SDU_PERCENT_X  60
#define SDU_PERCENT_Y  310
static int SDU_LAST_PERCENT;


class SDUpdater {
  public: 
    void updateFromFS(fs::FS &fs, String fileName = MENU_BIN );
    static void SDMenuProgress(int state, int size);
    void displayUpdateUI(String label);
  private:
    void performUpdate(Stream &updateSource, size_t updateSize, String fileName);
};

/* don't break older versions of the SD Updater */
static void updateFromFS(fs::FS &fs, String fileName = MENU_BIN ) {
  SDUpdater sdUpdater;
  sdUpdater.updateFromFS(fs, fileName);
}


void SDUpdater::displayUpdateUI(String label) {
  tft.setCursor(0, 300);
  tft.setTextColor(WROVER_WHITE);
  tft.fillRect(0, 300, SDU_PROGRESS_W+2, 20, WROVER_BLACK);
  tft.printf(label.c_str());
  tft.drawRect(SDU_PROGRESS_X, SDU_PROGRESS_Y, SDU_PROGRESS_W+2, SDU_PROGRESS_H, WROVER_WHITE);
}


void SDUpdater::SDMenuProgress(int state, int size) {
  uint32_t percent = (state*100) / size;
  if(SDU_LAST_PERCENT==percent) return;
  SDU_LAST_PERCENT = percent;
  Serial.printf("percent = %d\n", percent);
  uint16_t x = tft.getCursorX();
  uint16_t y = tft.getCursorY();

  uint16_t wpercent = (SDU_PROGRESS_W * percent) / 100;

  tft.setTextColor(WROVER_WHITE, WROVER_BLACK);

  if (percent > 0 && wpercent <= SDU_PROGRESS_W) {
    tft.fillRect(SDU_PROGRESS_X+1, SDU_PROGRESS_Y+1, wpercent, SDU_PROGRESS_H-2, WROVER_GREEN);
    tft.fillRect(SDU_PROGRESS_X+1+wpercent, SDU_PROGRESS_Y+1, SDU_PROGRESS_W-wpercent, SDU_PROGRESS_H-2, WROVER_BLACK);
  } else {
    tft.fillRect(SDU_PROGRESS_X+1, SDU_PROGRESS_Y+1, SDU_PROGRESS_W, SDU_PROGRESS_H-2, WROVER_BLACK);
  }
  tft.setCursor(SDU_PERCENT_X, SDU_PERCENT_Y);
  tft.print(" " + String(percent) + "% ");
  tft.setCursor(x, y);
}

// perform the actual update from a given stream
void SDUpdater::performUpdate(Stream &updateSource, size_t updateSize, String fileName) {
   displayUpdateUI("LOADING " + fileName);
   Update.onProgress(SDMenuProgress);
   if (Update.begin(updateSize)) {
      size_t written = Update.writeStream(updateSource);
      if (written == updateSize) {
         Serial.println("Written : " + String(written) + " successfully");
      } else {
         Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
      }
      if (Update.end()) {
         Serial.println("OTA done!");
         if (Update.isFinished()) {
            Out.println();
            Out.println("Update successfully completed");
            Out.println("Rebooting.");
            Out.println();
         } else {
            Out.println();
            Out.println("Update not finished!");
            Out.println("Something went wrong!");
            Out.println();
         }
      } else {
         Out.println();
         Out.println("Error Occurred. Error #: " + String(Update.getError()));
         Out.println();
      }
   } else {
      Out.println();
      Out.println("Not enough space to begin OTA");
      Out.println();
   }
}

// check given FS for valid menu.bin and perform update if available
void SDUpdater::updateFromFS(fs::FS &fs, String fileName) {
  File updateBin = fs.open(fileName);
  if (updateBin) {
    if(updateBin.isDirectory()){
      Out.println();
      Out.println("Error, "+ fileName +" is a directory");
      Out.println();
      updateBin.close();
      return;
    }
    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Out.println();
      Out.println("Updating from "+ fileName);
      Out.println();
      performUpdate(updateBin, updateSize, fileName);
    } else {
      Out.println();
      Out.println("Error, file "+ fileName +" is empty!");
      Out.println();
    }
    updateBin.close();
  } else {
    Out.println();
    Out.println("Could not load "+ fileName);
    Out.println();
  }
}
