/*\
 * SD Helpers
\*/

static bool sd_mounted = false;



bool M5StackSDBegin()
{
  #ifdef _SD_H_
    return M5.sd_begin();
    //return SD.begin(4, SPI, 20000000);
  #endif
  return false;
}


static const char* _FSFilePath( fs::File *file ) {
  #if defined ESP_IDF_VERSION_MAJOR && ESP_IDF_VERSION_MAJOR >= 4
    return file->path();
  #else
    return file->name();
  #endif
}


bool SDSetup()
{
  if(sd_mounted) return true;
  unsigned long max_wait = 500;
  byte attempts = 100;
  while ( sd_mounted == false && attempts>0) {
    if ( SD_begin ) {
      sd_mounted = true;
    } else {
      log_e("[SD] Mount Failed");
      delay(max_wait);
      if(attempts%2==0) {
        IconRender( &SDLoaderIcon, ICON_STATUS_disk00, (tft.width()-30)/2, 100 );
      } else {
        IconRender( &SDLoaderIcon, ICON_STATUS_disk01, (tft.width()-30)/2, 100 );
      }
      attempts--;
    }
  }
  if( attempts != 100 ) {
    //tft.fillRect( (tft.width()-30)/2, 100, 30, 30, Out.BgColor );
  }
  return sd_mounted;
}







static void listDirs(fs::FS &fs, const char * dirname, uint8_t levels, const char* needle=NULL)
{
  Serial.printf("\nListing directories: %s\n\n", dirname);

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
  //unsigned long totalSize = 0;
  char fileDate[64] = "1980-01-01 00:07:20";
  time_t lastWrite;
  struct tm * tmstruct;

  // show folders first
  while( file ) {
    lastWrite = file.getLastWrite();
    tmstruct = localtime(&lastWrite);

    sprintf(fileDate, "%04d-%02d-%02d %02d:%02d:%02d",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    if( (tmstruct->tm_year)+1900 < 2000 ) {
      // time is not set
    }
    if(file.isDirectory()) {
      Serial.printf( "    %-32s | %20s | DIRECTORY\n", _FSFilePath( &file ), fileDate );
    }
    file.close();
    file = root.openNextFile();
  }
  file.close();
  root.close();
}


static void listFiles(fs::FS &fs, const char * dirname, uint8_t levels, const char* needle=NULL)
{
  //Serial.printf("\nListing Files: %s\n\n", dirname);

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
  unsigned long totalSize = 0;
  char fileDate[64] = "1980-01-01 00:07:20";
  time_t lastWrite;
  struct tm * tmstruct;

  // show files (TODO: sort by date)
  while( file ) {

    lastWrite = file.getLastWrite();
    tmstruct = localtime(&lastWrite);

    sprintf(fileDate, "%04d-%02d-%02d %02d:%02d:%02d",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    if( (tmstruct->tm_year)+1900 < 2000 ) {
      // time is not set
    }

    if(!file.isDirectory()) {
      const char* fileName = _FSFilePath ( &file );
      unsigned long fileSize = file.size();
      if( needle!=NULL && strcmp( fileName, needle ) == 0 ) {
        Serial.printf( "*   %-32s | %20s | %8ld Bytes\n", fileName, fileDate, fileSize );
      } else {
        Serial.printf( "    %-32s | %20s | %8ld Bytes\n", fileName, fileDate, fileSize );
      }
      totalSize += fileSize;
    }/* else {
      Serial.printf( "    %-32s | %20s | DIRECTORY\n", file.name(), fileDate );
    }*/
    file.close();
    file = root.openNextFile();
  }
  file.close();
  root.close();

  Serial.printf("\nTotal space used: %ld Bytes\n\n", totalSize);
}


static void listDir(fs::FS &fs, const char * dirname, uint8_t levels, const char* needle=NULL)
{
  listDirs( fs, dirname, levels, needle );
  listFiles( fs, dirname, levels, needle );
}
