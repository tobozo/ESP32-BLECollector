// minimal spiffs partition size is required for that
#include "ESP32FtpServer.h"
#include <WiFi.h>

char WiFi_SSID[32];
char WiFi_PASS[32];



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

}

