
static bool sd_mounted = false;

bool SDSetup() {
  if(sd_mounted) return true;
  unsigned long max_wait = 500;
  byte attempts = 100;
  while ( sd_mounted == false && attempts>0) {
    if ( SD_begin() ) {
      sd_mounted = true;
    } else {
      log_e("[SD] Mount Failed");
      //delay(max_wait);
      if(attempts%2==0) {
        tft_drawJpg( disk00_jpg, disk00_jpg_len, (tft.width()-30)/2, 100, 30, 30);
      } else {
        tft_drawJpg( disk01_jpg, disk00_jpg_len, (tft.width()-30)/2, 100, 30, 30);
      }
      AmigaBall.animate( max_wait, false );
      attempts--;
    }
  }
  if( attempts != 100 ) {
    AmigaBall.animate( 1 );
    //tft.fillRect( (tft.width()-30)/2, 100, 30, 30, Out.BgColor );
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
  Serial.println("    NAME                             |                    |     SIZE");
  Serial.println("-------------------------------------------------------------------------");
  unsigned long totalSize = 0;
  char fileDate[64] = "1980-01-01 00:07:20";
  time_t lastWrite;
  struct tm * tmstruct;

  while( file ) {

    lastWrite = file.getLastWrite();
    tmstruct = localtime(&lastWrite);

    sprintf(fileDate, "%04d-%02d-%02d %02d:%02d:%02d",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    if( (tmstruct->tm_year)+1900 < 2000 ) {
      // time is not set
    }

    if(!file.isDirectory()) {
      const char* fileName = file.name();
      unsigned long fileSize = file.size();
      if( needle!=NULL && strcmp( fileName, needle ) == 0 ) {
        Serial.printf( "*   %-32s | %20s | %8d Bytes\n", fileName, fileDate, fileSize );
      } else {
        Serial.printf( "    %-32s | %20s | %8d Bytes\n", fileName, fileDate, fileSize );
      }
      totalSize += fileSize;
    } else {
      Serial.printf( "    %-32s | %20s | DIRECTORY\n", file.name(), fileDate );
    }
    file.close();
    file = root.openNextFile();
  }

  Serial.printf("\nTotal space used: %d Bytes\n\n", totalSize);
}
