// minimal spiffs partition size is required for that
#include "ESP32FtpServer.h"
#include <WiFi.h>

char WiFi_SSID[32];
char WiFi_PASS[32];


/*
static bool wifiRunning = false;

static void wifiOff() {
  WiFi.mode( WIFI_OFF );
  wifiRunning = false;
}

static void stubbornConnect() {
  uint8_t wifi_retry_count = 0;
  uint8_t max_retries = 3;
  unsigned long stubbornness_factor = 3000; // ms to wait between attempts

  #ifdef ESP32
    while (WiFi.status() != WL_CONNECTED && wifi_retry_count < 3)
  #else
    while (WiFi.waitForConnectResult() != WL_CONNECTED && wifi_retry_count < max_retries)
  #endif
  {
    WiFi.begin(); // put your ssid / pass if required, only needed once
    Serial.print(WiFi.macAddress());
    Serial.printf(" => WiFi connect - Attempt No. %d\n", wifi_retry_count+1);
    delay( stubbornness_factor );
    wifi_retry_count++;
  }

  if(wifi_retry_count >= 3) {
    Serial.println("no connection, forcing retry");
    wifiOff();
    stubbornConnect();
    return;
    //ESP.restart();
  }

  if (WiFi.waitForConnectResult() == WL_CONNECTED){
    Serial.println("Connected as");
    Serial.println(WiFi.localIP());
  }

  wifiRunning = true;

}*/


#include <HTTPClient.h>
#include <WiFiClientSecure.h>

HTTPClient http;

const char * headerKeys[] = {"location", "redirect"};
const size_t numberOfHeaders = 2;

void (*PrintProgressBar)(float progress, float magnitude);


bool /*yolo*/wget( const char* url, fs::FS &fs, const char* path ) {

  WiFiClientSecure *client = new WiFiClientSecure;
  client->setCACert( NULL ); // yolo security

  const char* UserAgent = "ESP32HTTPClient";

  http.setUserAgent( UserAgent );
  http.setConnectTimeout( 10000 ); // 10s timeout = 10000

  if( ! http.begin(*client, url ) ) {
    log_e("Can't open url %s", url );
    return false;
  }

  http.collectHeaders(headerKeys, numberOfHeaders);

  log_w("URL = %s", url);

  int httpCode = http.GET();

  // file found at server
  if (httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    String newlocation = "";
    for(int i = 0; i< http.headers(); i++) {
      String headerContent = http.header(i);
      if( headerContent !="" ) {
        newlocation = headerContent;
        Serial.printf("%s: %s\n", headerKeys[i], headerContent.c_str());
      }
    }

    http.end();
    if( newlocation != "" ) {
      log_w("Found 302/301 location header: %s", newlocation.c_str() );
      return wget( newlocation.c_str(), fs, path );
    } else {
      log_e("Empty redirect !!");
      return false;
    }
  }

  WiFiClient *stream = http.getStreamPtr();

  if( stream == nullptr ) {
    http.end();
    log_e("Connection failed!");
    return false;
  }

  File outFile = fs.open( path, FILE_WRITE );
  if( ! outFile ) {
    log_e("Can't open %s file to save url %s", path, url );
    return false;
  }

  uint8_t buff[512] = { 0 };
  size_t sizeOfBuff = sizeof(buff);
  int len = http.getSize();
  int bytesLeftToDownload = len;
  int bytesDownloaded = 0;

  while(http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if(size) {
      // read up to 512 byte
      int c = stream->readBytes(buff, ((size > sizeOfBuff) ? sizeOfBuff : size));
      outFile.write( buff, c );
      bytesLeftToDownload -= c;
      bytesDownloaded += c;
      Serial.printf("%d bytes left\n", bytesLeftToDownload );
      float progress = (((float)bytesDownloaded / (float)len) * 100.00);
      PrintProgressBar( progress, 100.0 );
    }
  }
  outFile.close();
  return fs.exists( path );
}

