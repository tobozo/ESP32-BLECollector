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
#include "esp_ota_ops.h"
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
    bool updateFromFS(fs::FS &fs, String fileName = MENU_BIN );
    static void SDMenuProgress(int state, int size);
    void displayUpdateUI(String label);
  private:
    bool performUpdate(Stream &updateSource, size_t updateSize, String fileName);
};

/* don't break older versions of the SD Updater */
static bool updateFromFS(fs::FS &fs, String fileName = MENU_BIN ) {
  SDUpdater sdUpdater;
  return sdUpdater.updateFromFS(fs, fileName);
}



#define SPI_FLASH_SEC_STEP8 SPI_FLASH_SEC_SIZE / 4
static uint8_t g8_rbuf[SPI_FLASH_SEC_STEP8];
static uint8_t g32_rbuf[SPI_FLASH_SEC_STEP8];

uint32_t sizeofneedle = strlen(needle);
uint32_t sizeoftrail = strlen(welcomeMessage) - sizeofneedle;

/* checks buffer for signature, returns true if found */
static bool parseBuffer(char* &signature) {
  for(uint32_t i=0;i<(SPI_FLASH_SEC_STEP8-sizeofneedle);i++) {
    uint32_t j;
    for(j=0;j<sizeofneedle;j++) {
      if((char)g8_rbuf[i+j]!=(char)needle[j]) {
        break;
      }
    }
    if(j==sizeofneedle) {
      for(uint32_t k=0;k<sizeoftrail;k++) {
        signature[k] = (char)g8_rbuf[i+j+k];
      }
      signature[sizeoftrail]='\0';
      return true;
    }
  }
  return false;
}


static char* getPartitionBuildSignature(const esp_partition_t* &currentpartition) {
  char *signature = (char*)malloc(sizeoftrail);
  for (uint32_t base_addr = currentpartition->address; base_addr < currentpartition->address + currentpartition->size; base_addr += SPI_FLASH_SEC_STEP8) {
    memset(g8_rbuf, 0, SPI_FLASH_SEC_STEP8);
    spi_flash_read(base_addr, g8_rbuf, SPI_FLASH_SEC_STEP8);
    if(parseBuffer(signature)) return signature;
    base_addr-=(SPI_FLASH_SEC_STEP8/2);
  }
  return signature;
}


static char* getBinarySignature(fs::FS &fs, String fileName ) {
  char *signature = (char*)malloc(sizeoftrail);
  if(!fs.exists(fileName)) {
    Serial.println("[FAIL] getBinarySignature() file not found: " + fileName);
    return signature;  
  }
  File binaryFile = fs.open(fileName);
  if(!binaryFile) {
    Serial.println("[FAIL] getBinarySignature() can't open file: " + fileName);
    binaryFile.close();
    return signature;      
  }
  memset(g8_rbuf, 0, SPI_FLASH_SEC_STEP8);
  uint32_t pos = 0;
  while(binaryFile.read(g8_rbuf, SPI_FLASH_SEC_STEP8)) {
     if(parseBuffer(signature)) {
      break;
     }
     pos+=SPI_FLASH_SEC_STEP8/2;
     binaryFile.seek(pos, SeekSet);
  }
  binaryFile.close();
  return signature;
}


static bool doUpdateToFS(fs::FS &fs, String fileName, const esp_partition_t* currentpartition) {
  uint8_t headerbuf[4];
  
  if(!currentpartition) {
    Out.println("Bad partition");
    return false;
  }
  if(!ESP.flashRead(currentpartition->address, (uint32_t*)headerbuf, 4)) {
    Out.println("Partition is not bootable");
    return false;
  }
  if(headerbuf[0] != 0xE9/*ESP_IMAGE_HEADER_MAGIC*/) {
    Out.println("Partition has no magic header");
    return false;
  }

  const char* partitionSignature = getPartitionBuildSignature( currentpartition );
  const char* binarySignature    = getBinarySignature( fs, fileName );
  if(strcmp(partitionSignature, binarySignature)==0) {
    return false;
  } else {
    Out.println("Partition signatures differ"); Out.println(partitionSignature); Out.println(binarySignature);
  }

  if(fs.remove(fileName)){
    Out.println("Outdated "+fileName+" deleted");
  }

  File updateBin = fs.open(fileName, FILE_WRITE);
  if(!updateBin) {
    Out.println("Can't write to " + String(fileName) + " :-(");
    updateBin.close();
    return false;
  }
  Out.println("Writing "+ String(fileName) +" ...");
  uint32_t bytescounter = 0;
  for (uint32_t base_addr = currentpartition->address; base_addr < currentpartition->address + currentpartition->size; base_addr += SPI_FLASH_SEC_STEP8) {
    memset(g8_rbuf, 0, SPI_FLASH_SEC_STEP8);
    spi_flash_read(base_addr, g8_rbuf, SPI_FLASH_SEC_STEP8);
    updateBin.write(g8_rbuf, SPI_FLASH_SEC_STEP8);
    bytescounter++;
    if(bytescounter%128==0) {
      Serial.println(".");
    } else {
      Serial.print(".");
    }
  }
  Out.println("Done");
  updateBin.close();
  return true;
}


static char* getSignature(byte sourcepartition) {
  if(sourcepartition==0) {
    const esp_partition_t* partition = esp_ota_get_running_partition();
    return getPartitionBuildSignature( partition );
  } else {
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    return getPartitionBuildSignature( partition );
  }    
}


static bool updateToFS(fs::FS &fs, String fileName, byte sourcepartition) {
  switch(sourcepartition) {
    case 0: return doUpdateToFS(fs, fileName,  esp_ota_get_running_partition()); break;
    case 1: return doUpdateToFS(fs, fileName,  esp_ota_get_next_update_partition(NULL)); break;
    default: return false;
  }
  return false;
}


static bool rollBackOrUpdateFromFS(fs::FS &fs, String fileName = MENU_BIN ) {
  const char* binarySignature = getBinarySignature( fs, fileName );
  const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
  const char* partitionSignature = getPartitionBuildSignature( partition );
  Serial.print("[rollBackOrUpdateFromFS] SD binary signature for " + fileName + " is: ");
  Serial.println( binarySignature );
  Serial.print("[rollBackOrUpdateFromFS] Next partition signature is: " );
  Serial.println( partitionSignature );
  if(strcmp(binarySignature, partitionSignature)==0) {
    Serial.println("[rollBackOrUpdateFromFS][INFO] Both signatures match");
    if( Update.canRollBack() )  {
      Serial.println("[rollBackOrUpdateFromFS][ROLLBACK OK] Using rollback update"); // much faster than re-flashing
      Update.rollBack();
      return true;
    } else {
      Serial.println("[rollBackOrUpdateFromFS][ROLLBACK FAIL] Looks like rollback is not possible");
    }
  } else {
    Serial.println("[rollBackOrUpdateFromFS][ROLLBACK FAIL] None of the signatures match");
  }
  SDUpdater sdUpdater;
  return sdUpdater.updateFromFS(fs, fileName);
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
bool SDUpdater::performUpdate(Stream &updateSource, size_t updateSize, String fileName) {
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
            Out.println(); Out.println("Update successfully completed"); Out.println("Rebooting."); Out.println();
            return true;
         } else {
            Out.println(); Out.println("Update not finished!"); Out.println("Something went wrong!"); Out.println();
            return false;
         }
      } else {
         Out.println(); Out.println("Error Occurred. Error #: " + String(Update.getError())); Out.println();
         return false;
      }
   } else {
      Out.println(); Out.println("Not enough space to begin OTA"); Out.println();
      return false;
   }
}

// check given FS for valid menu.bin and perform update if available
bool SDUpdater::updateFromFS(fs::FS &fs, String fileName) {
  File updateBin = fs.open(fileName);
  bool ret = false;
  if (updateBin) {
    if(updateBin.isDirectory()){
      Out.println(); Out.println("Error, "+ fileName +" is a directory"); Out.println();
      updateBin.close();
      return ret;
    }
    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Out.println(); Out.println("Updating from "+ fileName); Out.println();
      ret = performUpdate(updateBin, updateSize, fileName);
    } else {
      Out.println(); Out.println("Error, file "+ fileName +" is empty!"); Out.println();
    }
    updateBin.close();
  } else {
    Out.println(); Out.println("Could not load "+ fileName); Out.println();
  }
  return ret;
}
