
static bool sd_mounted = false;

bool SDSetup() {
  if(sd_mounted) return true;
  unsigned long max_wait = 500;
  byte attempts = 100;
  while ( sd_mounted == false && attempts>0) {
    if (BLE_FS.begin() ) {
      sd_mounted = true;
    } else {
      log_e("[SD] Mount Failed");
      //delay(max_wait);
      if(attempts%2==0) {
        tft.drawJpg( disk00_jpg, disk00_jpg_len, (tft.width()-30)/2, 100, 30, 30);
      } else {
        tft.drawJpg( disk01_jpg, disk00_jpg_len, (tft.width()-30)/2, 100, 30, 30);
      }
      AmigaBall.animate( max_wait, false );
      attempts--;
    }
  }
  if( attempts != 100 ) {
    AmigaBall.animate( 1 );
    tft.fillRect( (tft.width()-30)/2, 100, 30, 30, BGCOLOR );
  }
  return sd_mounted;
}

static void listDir() {
  // blah
}

static void listDir(fs::FS &fs, const char * dirname, uint8_t levels, const char* needle=NULL){
  Serial.printf("\nListing directory: %s\n\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  Serial.println("    NAME                             |     SIZE");
  Serial.println("-----------------------------------------------------");
  unsigned long totalSize = 0;
  while(file){
    if(!file.isDirectory()) {
      const char* fileName = file.name();
      unsigned long fileSize = file.size();
      if( needle!=NULL && strcmp( fileName, needle ) == 0 ) {
        Serial.printf("*   %-32s | %8d Bytes\n", fileName, fileSize);
      } else {
        Serial.printf("    %-32s | %8d Bytes\n", fileName, fileSize);
      }
      totalSize += fileSize;
    }
    file = root.openNextFile();
  }
  Serial.printf("\nTotal space used: %d Bytes\n\n", totalSize);
}


#ifdef NEEDS_SDUPDATER
  #include "SDUpdater.h" // multi roms system
#endif
