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
#define SPI_FLASH_SEC_STEP8 SPI_FLASH_SEC_SIZE / 4
#define SPI_SD_SEC_STEP 512
static uint8_t spi_rbuf[SPI_FLASH_SEC_STEP8];
static uint8_t sd_rbuf[SPI_SD_SEC_STEP];
const char* outTpl = " [%s] %s";
char outStr[32] = {'\0'};


enum OTAPartitionNames {
  NO_PARTITION = -1,
  CURRENT_PARTITION = 0,
  NEXT_PARTITION = 1
};


class SDUpdater {
  public:
    bool updateFromFS(fs::FS &fs, const char* fileName);
    static void SDMenuProgress(int state, int size);
    void displayUpdateUI(String label);
  private:
    bool performUpdate(Stream &updateSource, size_t updateSize, const char* fileName);
};

/* don't break older versions of the SD Updater */
static bool updateFromFS(fs::FS &fs, const char* fileName ) {
  SDUpdater sdUpdater;
  return sdUpdater.updateFromFS(fs, fileName);
}



/* checks buffer for signature, returns true if found */
static bool parseBuffer(char* signature, const uint8_t *rbuf, uint16_t buffer_size) {
  for(uint32_t i=0;i<(buffer_size-sizeofneedle);i++) {
    uint32_t j;
    for(j=0;j<sizeofneedle;j++) {
      if((char)rbuf[i+j]!=(char)needle[j]) {
        break;
      }
    }
    if(j==sizeofneedle) {
      for(uint32_t k=0;k<sizeoftrail;k++) {
        signature[k] = (char)rbuf[i+j+k];
      }
      signature[sizeoftrail]='\0';
      return true;
    }
  }
  return false;
}

static void getFlashSignature(char* signature, const esp_partition_t* &currentpartition) {
  for (uint32_t base_addr = currentpartition->address; base_addr < currentpartition->address + currentpartition->size; base_addr += SPI_FLASH_SEC_STEP8) {
    memset(spi_rbuf, 0, SPI_FLASH_SEC_STEP8);
    spi_flash_read(base_addr, spi_rbuf, SPI_FLASH_SEC_STEP8);
    if(parseBuffer(signature, spi_rbuf, SPI_FLASH_SEC_STEP8)) {
      log_d("[FOUND partition build signature '%s' at offset %d]", signature, base_addr);
      return;
    }
    base_addr-=(SPI_FLASH_SEC_STEP8/2);
  }
  log_e("[FAILED to find partition build signature at address %d (parsed %d bytes)]", currentpartition->address, currentpartition->size);
}

static void getBinarySignature( char *signature, fs::FS &fs, const char* fileName ) {
  if(!fs.exists(fileName)) {
    log_e("[FAIL] getBinarySignature() file not found: %s", fileName );
    return;  
  }
  File binaryFile = fs.open(fileName);
  if(!binaryFile) {
    log_e("[FAIL] getBinarySignature() can't open file: ", fileName );
    binaryFile.close();
    return;      
  }
  memset(sd_rbuf, 0, SPI_SD_SEC_STEP);
  uint32_t pos = 0;
  while(binaryFile.read(sd_rbuf, SPI_SD_SEC_STEP)) {
     if(parseBuffer(signature, sd_rbuf, SPI_SD_SEC_STEP)) {
       binaryFile.close();
       log_d("[FOUND SD binary signature '%s' for %s (parsed %d bytes)]", signature, fileName, pos);
       return;
     }
     pos+=SPI_SD_SEC_STEP/2;
     binaryFile.seek(pos, SeekSet);
  }
  log_e("[FAILED to find SD binary signature for %s (parsed %d bytes)]", fileName, pos);
  binaryFile.close();
}

static bool doUpdateToFS(fs::FS &fs, const char* fileName, const esp_partition_t* currentpartition) {
  uint8_t headerbuf[4];
  
  if(!currentpartition) {
    Serial.println(" [ERROR] Bad partition");
    return false;
  }
  if(!ESP.flashRead(currentpartition->address, (uint32_t*)headerbuf, 4)) {
    Serial.println(" [ERROR] Partition is not bootable");
    return false;
  }
  if(headerbuf[0] != 0xE9/*ESP_IMAGE_HEADER_MAGIC*/) {
    Serial.println(" [ERROR] Partition has no magic header");
    return false;
  }

  char *flashSignature = (char*)malloc(sizeofneedle+1);
  char *SDSignature    = (char*)malloc(sizeofneedle+1);
  getFlashSignature( flashSignature, currentpartition );
  getBinarySignature( SDSignature, fs, fileName );
  if(strcmp(flashSignature, SDSignature)==0) {
    log_d("No update is needed");
    return false;
  } else {
    Serial.println(" [INFO] Partition signatures differ"); 
    sprintf( outStr, outTpl, "FL", flashSignature);
    Serial.println( outStr ); 
    sprintf( outStr, outTpl, "SD", SDSignature);
    Serial.println( outStr ); 
  }

  if(fs.remove(fileName)){
    Serial.println(String(" [INFO] Outdated "+String(fileName)+" deleted").c_str());
  }

  File updateBin = fs.open(fileName, FILE_WRITE);
  if(!updateBin) {
    Serial.println(String(" [ERROR] Can't write to " + String(fileName) + " :-(").c_str());
    updateBin.close();
    return false;
  }
  Serial.println(String(" [INFO] Writing "+ String(fileName) +" ...").c_str());
  uint32_t bytescounter = 0;
  for (uint32_t base_addr = currentpartition->address; base_addr < currentpartition->address + currentpartition->size; base_addr += SPI_FLASH_SEC_STEP8) {
    memset(spi_rbuf, 0, SPI_FLASH_SEC_STEP8);
    spi_flash_read(base_addr, spi_rbuf, SPI_FLASH_SEC_STEP8);
    updateBin.write(spi_rbuf, SPI_FLASH_SEC_STEP8);
    bytescounter++;
    if(bytescounter%128==0) {
      Serial.println(".");
    } else {
      Serial.print(".");
    }
  }
  Serial.println(" [INFO] Done");
  updateBin.close();
  return true;
}


static void getFactoryPartition() {
  esp_partition_iterator_t pi = esp_partition_find( ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL );
  if(pi != NULL) {
    const esp_partition_t* factory = esp_partition_get(pi);
    esp_partition_iterator_release(pi);
    if(esp_ota_set_boot_partition(factory) == ESP_OK) {
      //esp_restart();
    }
  }
}


static void getSignature(char* signature, byte sourcepartition) {
  if(sourcepartition==0) {
    const esp_partition_t* partition = esp_ota_get_running_partition();
    getFlashSignature( signature, partition );
  } else {
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    getFlashSignature( signature, partition );
  }    
}

static bool updateToFS(fs::FS &fs, const char* fileName, byte sourcepartition) {
  switch(sourcepartition) {
    case 0: return doUpdateToFS(fs, fileName,  esp_ota_get_running_partition()); break;
    case 1: return doUpdateToFS(fs, fileName,  esp_ota_get_next_update_partition(NULL)); break;
    default: return false;
  }
  return false;
}

static bool rollBackOrUpdateFromFS(fs::FS &fs, const char* fileName ) {
  const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
  char *partitionSignature = (char*)malloc(sizeofneedle+1);
  char *binarySignature    = (char*)malloc(sizeofneedle+1);
  getBinarySignature( binarySignature, fs, fileName );
  getFlashSignature( partitionSignature, partition );
  log_d("[rollBackOrUpdateFromFS] SD binary signature for %s is %s", fileName, binarySignature );
  log_d("[rollBackOrUpdateFromFS] Next partition signature is: %s", partitionSignature );
  if(strcmp(binarySignature, partitionSignature)==0) {
    log_d("[rollBackOrUpdateFromFS][INFO] Both signatures match");
    if( Update.canRollBack() )  {
      log_d("[rollBackOrUpdateFromFS][ROLLBACK OK] Using rollback update"); // much faster than re-flashing
      Update.rollBack();
      return true;
    } else {
      log_e("[rollBackOrUpdateFromFS][ROLLBACK FAIL] Looks like rollback is not possible");
    }
  } else {
    log_e("[rollBackOrUpdateFromFS][ROLLBACK FAIL] None of the signatures match");
  }
  SDUpdater sdUpdater;
  return sdUpdater.updateFromFS(fs, fileName);
}

void selfReplicateToSD() {
  // mirror current binary to SD Card if needed
  char *currentMenuSignature = (char*)malloc(sizeofneedle+1);
  char *nextMenuSignature    = (char*)malloc(sizeofneedle+1);

  char *binaryNTPSignature   = (char*)malloc(sizeofneedle+1);
  char *binaryBLESignature   = (char*)malloc(sizeofneedle+1);

  getSignature( currentMenuSignature, CURRENT_PARTITION );
  getSignature( nextMenuSignature, NEXT_PARTITION );
  getBinarySignature( binaryNTPSignature, BLE_FS, "/NTPMenu.bin" );
  getBinarySignature( binaryBLESignature, BLE_FS, "/BLEMenu.bin" );

  if( strcmp( BUILDSIGNATURE, currentMenuSignature ) == 0 ) {
    // Build signature matches with current partition, looks fine!
    char *binarySignature = (char*)malloc(sizeofneedle+1);
    getBinarySignature( binarySignature, BLE_FS, MENU_FILENAME );
    if( strcmp(binarySignature, currentMenuSignature)==0 ) {
      // Perfect match, nothing to do \o/
      log_d("[SD==FLASH]");
      return;
    } else {
      log_d("[SD!=FLASH] '%s' does not match '%s'", binarySignature, currentMenuSignature);
      Serial.println(" [SD!=FLASH]"); // mirror current flash partition to SD
      updateToFS(BLE_FS, MENU_FILENAME, CURRENT_PARTITION);
      //Out.scrollNextPage();
    }
  } else {
    log_e("[WUT] Build signature matches neither current nor next partition");
    log_e("[WUT] Current Built-in Signature is: %s", BUILDSIGNATURE );
    log_e("[WUT] Current Partition Signature is: %s", currentMenuSignature );
    log_e("[WUT] Next Partition Signature is: %s", nextMenuSignature );
  }
}


void SDUpdater::displayUpdateUI(String label) {
  tft.setCursor(0, 300);
  tft.setTextColor(BLE_WHITE);
  tft.fillRect(0, 300, SDU_PROGRESS_W+2, SDU_PROGRESS_H+20, BLE_BLACK);
  tft.printf(label.c_str());
  tft.drawRect(SDU_PROGRESS_X, SDU_PROGRESS_Y, SDU_PROGRESS_W+2, SDU_PROGRESS_H, BLE_WHITE);
}


void SDUpdater::SDMenuProgress(int state, int size) {
  uint32_t percent = (state*100) / size;
  if(SDU_LAST_PERCENT==percent) return;
  SDU_LAST_PERCENT = percent;
  log_d("percent = %d", percent);
  uint16_t x = tft.getCursorX();
  uint16_t y = tft.getCursorY();
  uint16_t wpercent = (SDU_PROGRESS_W * percent) / 100;
  tft.setTextColor(BLE_WHITE, BLE_BLACK);
  if (percent > 0 && wpercent <= SDU_PROGRESS_W) {
    tft.fillRect(SDU_PROGRESS_X+1, SDU_PROGRESS_Y+1, wpercent, SDU_PROGRESS_H-2, BLE_GREEN);
    tft.fillRect(SDU_PROGRESS_X+1+wpercent, SDU_PROGRESS_Y+1, SDU_PROGRESS_W-wpercent, SDU_PROGRESS_H-2, BLE_BLACK);
  } else {
    tft.fillRect(SDU_PROGRESS_X+1, SDU_PROGRESS_Y+1, SDU_PROGRESS_W, SDU_PROGRESS_H-2, BLE_BLACK);
  }
  tft.setCursor(SDU_PERCENT_X, SDU_PERCENT_Y);
  tft.print(" " + String(percent) + "% ");
  tft.setCursor(x, y);
}

// perform the actual update from a given stream
bool SDUpdater::performUpdate(Stream &updateSource, size_t updateSize, const char* fileName) {
   displayUpdateUI("LOADING " + String(fileName));
   Update.onProgress(SDMenuProgress);
   if (Update.begin(updateSize)) {
      size_t written = Update.writeStream(updateSource);
      if (written == updateSize) {
         log_d("Written : %s successfully", written);
      } else {
         log_w("Written only : %d of %d", written, updateSize);
      }
      if (Update.end()) {
         log_d("OTA done!");
         if (Update.isFinished()) {
            Serial.println(" [INFO] Update successful"); Serial.println(" [INFO] Rebooting.");
            return true;
         } else {
            Serial.println(" [ERROR] Update failed!"); Serial.println(" [ERROR] Something went wrong!");
            return false;
         }
      } else {
         Serial.println(String(" [ERROR] " + String(Update.getError())).c_str());
         return false;
      }
   } else {
      Serial.println(" [ERROR] Not enough space to begin OTA");
      return false;
   }
}

// check given FS for valid menu.bin and perform update if available
bool SDUpdater::updateFromFS(fs::FS &fs, const char* fileName) {
  File updateBin = fs.open(fileName);
  bool ret = false;
  if (updateBin) {
    if(updateBin.isDirectory()){
      Serial.println(String(" [ERROR] "+ String(fileName) +" is a directory").c_str());
      updateBin.close();
      return ret;
    }
    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Serial.println(String(" [INFO] Updating from "+ String(fileName)).c_str());
      ret = performUpdate(updateBin, updateSize, fileName);
    } else {
      Serial.println(String(" [ERROR]  file "+ String(fileName) +" is empty!").c_str());
    }
    updateBin.close();
  } else {
    Serial.println(String(" [ERROR] Could not load "+ String(fileName)).c_str());
  }
  return ret;
}
