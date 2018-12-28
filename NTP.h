

  HTTPClient http;
  SDUpdater sdUpdater;
  
  #ifdef WIFI_SSID // see Settings.h
    #define WiFi_Begin() WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  #else
    #define WiFi_Begin() WiFi.begin();
  #endif

  //const char* TZ_INFO = "CET-1CEST,M3.3.0,M10.5.0/3"; // build your TZ String here e.g. https://phpsecu.re/tz/get.php?timezone=Europe/Paris
  // Paris, France = CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00 // or pick it from here https://sites.google.com/a/usapiens.com/opnode/time-zones
  boolean NTPSyncEventTriggered = false; // True if a time even has been triggered
  NTPSyncEvent_t ntpEvent; // Last triggered event
  boolean RTCAdjusted = false;
  
  
  // used to retrieve DB files from the webs :]
  void wget(String bin_url, String appName, const char* &ca ) {
    log_d("Will check %s and save to SD as %s if filesizes differ", bin_url.c_str(), appName.c_str());
    http.begin(bin_url, ca);
    int httpCode = http.GET();
    if(httpCode <= 0) {
      log_e("[HTTP] GET... failed");
      http.end();
      return;
    }
    if(httpCode != HTTP_CODE_OK) {
      log_e("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str() ); 
      http.end();
      return;
    }
    int len = http.getSize();
    if(len<=0) {
      log_e("Failed to read %s content is empty, aborting", bin_url.c_str() );
      http.end();
      return;
    }
    int httpSize = len;
    uint8_t buff[512] = { 0 };
    WiFiClient * stream = http.getStreamPtr();
    File myFile = BLE_FS.open(appName, FILE_WRITE);
    if(!myFile) {
      log_e("Failed to open %s for writing, aborting", appName.c_str() );
      http.end();
      myFile.close();
      return;
    }
    while(http.connected() && (len > 0 || len == -1)) {
      sdUpdater.SDMenuProgress((httpSize-len)/10, httpSize/10);
      // get available data size
      size_t size = stream->available();
      if(size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // write it to SD
        myFile.write(buff, c);
        //Serial.write(buff, c);
        if(len-c >= 0) {
          len -= c;
        }
      }
      delay(1);
    }
    myFile.close();
    log_d("Copy done...");
    http.end();
  }

  // HEAD request to check for remote file size
  size_t getRemoteFileSize( String bin_url, const char * &ca) {
    http.begin(bin_url, ca);
    int httpCode = http.sendRequest("HEAD");
    if(httpCode <= 0) {
      log_e("[HTTP] HEAD... failed");
      http.end();
      return 0;
    }
    size_t len = http.getSize();
    http.end();
    return len;
  }

  // check for remote file size and retrieve if sizes differ
  void sudoMakeMeASandwich(String fileURL, const char* fileName, bool force = false) {
    size_t sdFileSize = 0;
    size_t remoteFileSize = 0;
    File fileBin = BLE_FS.open(fileName);
    if (fileBin) {
      sdFileSize = fileBin.size();
      log_d("[SD] %s : %d bytes", fileName.c_str(), sdFileSize);
      fileBin.close();
    } else {
      log_e("[SD] %s not found, will proceed to initial download", fileName);
    }
    remoteFileSize = getRemoteFileSize( fileURL, raw_github_ca );
    log_d("Remote file size is : %d", remoteFileSize);
    if( remoteFileSize == 0 ) return; // shit happens
    if( remoteFileSize != sdFileSize || force ) {
      wget( fileURL, fileName, raw_github_ca );
    }
  }

  // very stubborn wifi connect
  bool WiFiConnect() {
    unsigned long init_time = millis();
    unsigned long last_attempt = millis();
    unsigned long max_wait = 10000;
    byte attempts = 5;
    btStop();
    WiFi_Begin();
    while(WiFi.status() != WL_CONNECTED && attempts>0) {
      if( last_attempt + max_wait < millis() ) {
        attempts--;            
        last_attempt = millis();
        Out.println( String("[WiFi] Restarting ("+String(attempts)+" attempts left)").c_str() );
        WiFi.mode(WIFI_OFF);
        WiFi_Begin();
      }
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    if(WiFi.status() == WL_CONNECTED) {
      Out.println( String("[WiFi] Got an IP Address:" + WiFi.localIP().toString()).c_str() );
    } else {
      Out.println("[WiFi] No IP Address");
    }
    return WiFi.status() == WL_CONNECTED;
  }

  void NTPSyncEvent(NTPSyncEvent_t ntpEvent) {
    if( ntpEvent ) {
      Serial.print ("Time Sync error: ");
      if( ntpEvent == noResponse ) {
        Serial.println ("NTP server not reachable");
        ESP.restart();
      } else if( ntpEvent == invalidAddress ) {
        Serial.println ("Invalid NTP server address");
        ESP.restart();
      }
    } else {
      delay(100);
      time_t nowUnixTime = NTP.getTime();
      if( nowUnixTime > 1 ) {
        Serial.print ("Got NTP time: ");
        Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
        dumpTime( "Collected/translated NTP time", nowUnixTime );
        Serial.printf("Adjusting RTC from unixtime %d\n", nowUnixTime);
        RTC.adjust( nowUnixTime );
        RTCAdjusted = true;
      } else {
        Serial.print ("Got BOGUS time: ");
        Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
        dumpTime( "Collected/translated NTP time", nowUnixTime );
        ESP.restart();
      }
    }
  }

  void NTPSync() {
    if(WiFiConnect()) {
      NTP.begin ( NTP_SERVER, timeZone, true, minutesTimeZone );
      NTP.setInterval(63);
      while( !RTCAdjusted ) {
        if( NTPSyncEventTriggered ) {
          NTPSyncEvent(ntpEvent);
          NTPSyncEventTriggered = false;
        }
        delay(0);
      }
      DateTime ExternalDateTime = RTC.now();
      log_e("[External RTC Time Set to]: %04d-%02d-%02d %02d:%02d:%02d", 
        ExternalDateTime.year(),
        ExternalDateTime.month(),
        ExternalDateTime.day(),
        ExternalDateTime.hour(),
        ExternalDateTime.minute(),
        ExternalDateTime.second()
      );
      logTimeActivity(SOURCE_NTP, ExternalDateTime.unixtime());
      http.setReuse(true);
      sudoMakeMeASandwich("https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/master/SD/mac-oui-light.db", "/mac-oui-light.db");
      delay( 500 );
      sudoMakeMeASandwich("https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/master/SD/ble-oui.db", "/ble-oui.db");
    } else {
      // give up
      Out.println("[WiFi] No NTP Sync, regressing to ROGUE mode");
      // regress to ROGUE mode
      logTimeActivity(SOURCE_NONE, 0);
    }
    rollBackOrUpdateFromFS( BLE_FS, BLE_MENU_FILENAME );
    ESP.restart();
  }

  
  void NTPSetup() {
    NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
      ntpEvent = event;
      NTPSyncEventTriggered = true;
    });
    NTPSync();
  }
  
