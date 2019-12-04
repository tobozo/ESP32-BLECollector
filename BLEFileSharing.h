/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  Source: https://github.com/tobozo/ESP32-BLECollector

  MIT License

  Copyright (c) 2019 tobozo

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

#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"
#include "BLEScan.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#define TICKS_TO_DELAY 1000

static BLEUUID FileSharingServiceUUID("f59f6622-1540-0001-8d71-362b9e155667"); // service
static BLEUUID FileSharingWriteUUID(  "f59f6622-1540-0002-8d71-362b9e155667"); // characteristic to write file_chunk locally
static BLEUUID FileSharingRouteUUID("f59f6622-1540-0003-8d71-362b9e155667"); // characteristic to manage routing

BLEServer*         FileSharingServer;
BLEClient*         FileSharingClient;

BLEService*        FileSharingService;
BLERemoteService*  FileSharingRemoteService;

BLEAdvertising*    FileSharingAdvertising;

BLECharacteristic* FileSharingWriteChar;
BLECharacteristic* FileSharingRouteChar;

BLERemoteCharacteristic* FileSharingReadRemoteChar;
BLERemoteCharacteristic* FileSharingRouterRemoteChar;

extern esp_ble_addr_type_t pClientType;
extern std::string stdBLEAddress;

static int binary_file_length = 0;
static fs::File FileReceiver;
static size_t   FileReceiverExpectedSize = 0;
static size_t   FileReceiverReceivedSize = 0;
static size_t   FileReceiverProgress = 0;

static bool isFileSharingClientConnected = false;
static bool fileSharingServerTaskIsRunning = false;
static bool fileSharingServerTaskShouldStop = false;
static bool fileSharingServerCreated = false;
static bool fileSharingSendFileError = false;
static bool fileSharingServerIsRunning = false;

const char *sizeMarker = "size:";
const char* closeMessage = "close";

// helper
char *substr(char *src,int pos,int len) {
  char* dest = NULL;
  if (len>0) {
    dest = (char*)calloc(len+1, 1);
    if(NULL != dest) {
      strncat(dest,src+pos,len);
    }
  }
  return dest;
}

// BFS = BLE File Sharing

/******************************************************
  BLE File Receiver Methods
******************************************************/

void FileSharingReceiveFile( const char* filename ) {
  FileReceiverReceivedSize = 0;
  FileReceiverProgress = 0;
  FileReceiver = BLE_FS.open( filename, FILE_WRITE );
  if( !FileReceiver ) {
    log_e("Failed to create %s", filename);
  }
  log_v("Successfully opened %s for writing", filename);
}


void FileSharingCloseFile() {
  if( !FileReceiver ) {
    log_e("Nothing to close!");
    return;
  }
  takeMuxSemaphore();
  FileReceiver.close();
  if( FileReceiverReceivedSize != FileReceiverExpectedSize ) {
    log_e("Total size != expected size ( %d != %d )", FileReceiver.size(), FileReceiverExpectedSize);
    Out.println( "Copy Failed, please try again." );
  } else {
    Out.println( "Copy successful!" );
  }
  giveMuxSemaphore();
  //TODO: sha256_sum
  FileReceiverExpectedSize = 0;
  FileReceiverReceivedSize = 0;
  FileReceiverProgress = 0;
}


class FileSharingWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite( BLECharacteristic* WriterAgent ) {
    size_t len = WriterAgent->getDataLength();
    if( FileReceiverExpectedSize == 0 ) {
      // no size was previously sent, can't calculate
      log_e("Ignored %d bytes", len);
      return;
    }
    size_t progress = (((float)FileReceiverReceivedSize / (float)FileReceiverExpectedSize)*100.00);
    if( FileReceiver ) {
      FileReceiver.write( WriterAgent->getData(), len );
      log_e("Wrote %d bytes", len);
      FileReceiverReceivedSize += len;
    } else {
      // file write problem ?
      log_e("Ignored %d bytes", len);
    }
    if( FileReceiverProgress != progress ) {
      takeMuxSemaphore();
      UI.PrintProgressBar( (Out.width * progress)/100 );
      giveMuxSemaphore();
      FileReceiverProgress = progress;
      vTaskDelay(10);
    }
  }
};


class FileSharingRouteCallbacks : public BLECharacteristicCallbacks {
  void onWrite( BLECharacteristic* RouterAgent ) {
    char routing[512] = {0};
    memcpy( &routing, RouterAgent->getData(), RouterAgent->getDataLength() );
    log_v("Received copy routing query: %s", routing);
    if( strstr(routing, sizeMarker) ) { // messages starting with "size:"
      if( strlen( routing ) > strlen( sizeMarker ) ) {
        char* lenStr = substr( routing, strlen(sizeMarker), strlen(routing)-strlen(sizeMarker) );
        FileReceiverExpectedSize = atoi( lenStr );
        log_v( "Assigned size_t %d", FileReceiverExpectedSize );
      }
    } else if( strcmp( routing, closeMessage ) == 0 ) { // file end
      FileSharingCloseFile();
    } else { // filenames
      if( strcmp( routing, "/" BLE_VENDOR_NAMES_DB_FILE ) == 0 ) {
        FileSharingReceiveFile( "/" BLE_VENDOR_NAMES_DB_FILE );
      }
      if( strcmp( routing, "/" MAC_OUI_NAMES_DB_FILE ) == 0 ) {
        FileSharingReceiveFile( "/" MAC_OUI_NAMES_DB_FILE);
      }
      takeMuxSemaphore();
      UI.PrintProgressBar( 0 );
      giveMuxSemaphore();
    }
    takeMuxSemaphore();
    Out.println( routing );
    Out.println();
    giveMuxSemaphore();

  }
};


class FileSharingCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* SharingServer,  esp_ble_gatts_cb_param_t *param){
    log_v("A client is connected, stopping advertising");
    isFileSharingClientConnected = true;
    UI.headerStats("Connected :-)");
    takeMuxSemaphore();
    Out.println( "Client connected!" );
    Out.println();
    giveMuxSemaphore();
    BLEDevice::getAdvertising()->stop();
    // do some voodo on remote MTU for transfert perfs (thanks @chegewara)
    esp_ble_gap_set_prefer_conn_params(param->connect.remote_bda, 6, 6, 0, 500);
  }
  void onDisconnect(BLEServer* SharingServer) {
    log_v("A client disconnected, restarting advertising");
    isFileSharingClientConnected = false;
    UI.headerStats("Advertising (_x_)");
    takeMuxSemaphore();
    Out.println( "Client disconnected" );
    Out.println();
    giveMuxSemaphore();
    BLEDevice::startAdvertising();
  }
};


FileSharingCallbacks *FileSharingCallback;
FileSharingRouteCallbacks *FileSharingRouteCallback;
FileSharingWriteCallbacks *FileSharingWriteCallback;

// server as a slave service: wait for an upload signal
static void FileSharingServerTask(void* p) {

  BLEDevice::setMTU(517);
  FileSharingAdvertising = BLEDevice::getAdvertising();

  if( FileSharingCallback == NULL ) {
    FileSharingCallback = new FileSharingCallbacks();
  }
  if( FileSharingRouteCallback == NULL ) {
    FileSharingRouteCallback = new FileSharingRouteCallbacks();
  }
  if( FileSharingWriteCallback == NULL ) {
    FileSharingWriteCallback = new FileSharingWriteCallbacks();
  }

  if( !fileSharingServerCreated ) {
    FileSharingServer = BLEDevice::createServer();
    FileSharingServer->setCallbacks( FileSharingCallback );

    FileSharingService = FileSharingServer->createService( FileSharingServiceUUID );

    FileSharingWriteChar = FileSharingService->createCharacteristic( FileSharingWriteUUID, BLECharacteristic::PROPERTY_WRITE_NR); // PROPERTY_WRITE_NR for more speed
    FileSharingRouteChar = FileSharingService->createCharacteristic( FileSharingRouteUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE); // BLECharacteristic::PROPERTY_NOTIFY

    FileSharingRouteChar->setCallbacks( FileSharingRouteCallback );
    FileSharingWriteChar->setCallbacks( FileSharingWriteCallback );

    fileSharingServerCreated = true;
  }

  BLE2902* FileSharingRouteDescriptor = new BLE2902();
  FileSharingRouteDescriptor->setNotifications(true);
  FileSharingRouteChar->addDescriptor( FileSharingRouteDescriptor );

  FileSharingService->start();

  FileSharingAdvertising->addServiceUUID( FileSharingServiceUUID );
  FileSharingAdvertising->setMinInterval(0x100);
  FileSharingAdvertising->setMaxInterval(0x200);

  BLEDevice::startAdvertising();
  fileSharingServerTaskIsRunning = true;

  Serial.println("FileSharingClientTask up an advertising");
  UI.headerStats("Advertising (_x_)");
  takeMuxSemaphore();
  Out.println();
  Out.println( "Waiting for a BLE peer to send the files" );
  Out.println();
  giveMuxSemaphore();

  while(1) {
    if( fileSharingServerTaskShouldStop ) { // stop signal from outside the task
      BLEDevice::getAdvertising()->stop();
      FileSharingService->stop();
      fileSharingServerTaskIsRunning = false;
      fileSharingServerTaskShouldStop = false;
      fileSharingServerCreated = false;

      delete( FileSharingCallback);      FileSharingCallback = NULL;
      delete( FileSharingRouteCallback); FileSharingRouteCallback = NULL;
      delete( FileSharingWriteCallback); FileSharingWriteCallback = NULL;
      
      vTaskDelete( NULL );
    } else {
      vTaskDelay(100);
    }
  }
} // FileSharingClientTask








/******************************************************
  BLE File Sender Methods
******************************************************/

void FileSharingSendFile( const char* filename ) {
  fileSharingSendFileError = false;
  File fileToTransfert = BLE_FS.open( filename );

  if( !fileToTransfert ) {
    log_e("Can't open %s for reading", filename);
    fileSharingSendFileError = true;
    return;
  }
  size_t totalsize = fileToTransfert.size();
  size_t progress = totalsize;

  // send file size as string
  char myTotalSize[32];
  sprintf( myTotalSize, "%s%d", sizeMarker, totalsize );
  FileSharingRouterRemoteChar->writeValue((uint8_t*)myTotalSize, strlen(myTotalSize), false);
  
  #define BLE_FILECOPY_BUFFSIZE 512
  uint8_t buff[BLE_FILECOPY_BUFFSIZE];
  uint32_t len = fileToTransfert.read( buff, BLE_FILECOPY_BUFFSIZE );
  log_v("Starting transfert...");
  UI.headerStats(filename);
  takeMuxSemaphore();
  UI.PrintProgressBar( 0 );
  giveMuxSemaphore();
  int lastpercent = 0;
  while( len > 0 ) {
    progress-=len;
    int percent = 100 - (((float)progress/(float)totalsize)*100.00);
    if( !FileSharingReadRemoteChar->writeValue((uint8_t*)&buff, len, false) ) {
      // transfert failed !
      log_e("Failed to send %d bytes (%d percent done) %d / %d", len, percent, progress, totalsize);
      fileSharingSendFileError = true;
      break;
    } else {
      log_v("SUCCESS sending %d bytes (%d percent done) %d / %d", len, percent, progress, totalsize);
    }
    if( lastpercent != percent ) {
      takeMuxSemaphore();
      UI.PrintProgressBar( (Out.width * percent)/100 ); 
      giveMuxSemaphore();
      lastpercent = percent;
      vTaskDelay(10);
    }
    len = fileToTransfert.read( buff, BLE_FILECOPY_BUFFSIZE );
    vTaskDelay(10);
  }
  takeMuxSemaphore();
  UI.PrintProgressBar( Out.width );
  
  giveMuxSemaphore();
  
  UI.headerStats("[OK]");
  log_v("Transfert finished!");
  fileToTransfert.close();
}


static void FileSharingRouterCallbacks( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify ) {
  char routing[512] = {0};
  memcpy( &routing, pData, length );
  log_v("Received routing query: %s", routing);
  if (strcmp(routing, "/" BLE_VENDOR_NAMES_DB_FILE)==0) {
    FileSharingSendFile( "/" BLE_VENDOR_NAMES_DB_FILE );
  }
  if (strcmp(routing, "/" MAC_OUI_NAMES_DB_FILE)==0) {
    FileSharingSendFile( "/" MAC_OUI_NAMES_DB_FILE );
  }
};


class FileSharingClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pC){
    log_v("[Heap: %06d] Connect!!", freeheap);
  }
  void onDisconnect(BLEClient* pC) {
    log_v("[Heap: %06d] Disconnect!!", freeheap); 
  }
};


static void FileSharingClientTask( void * param ) {
  if( fileSharingServerIsRunning ) {
    log_w("Received FileSharingClientTask but server is already running!");
    vTaskDelete( NULL );
    return;
  }
  fileSharingServerIsRunning = true;

  BLEDevice::setMTU(517);
  FileSharingClient  = BLEDevice::createClient();
  FileSharingClient->setClientCallbacks( new FileSharingClientCallback() );

  //HasBTTime = false;
  log_v("[Heap: %06d] Will connect to address %s", freeheap, stdBLEAddress.c_str());
  if( !FileSharingClient->connect( stdBLEAddress, pClientType ) ) {
    log_e("[Heap: %06d] Failed to connect to address %s", freeheap, stdBLEAddress.c_str());
    UI.headerStats("Connect failed :-(");
    fileSharingServerIsRunning = false;
    vTaskDelete( NULL );
    return;
  }
  log_e("[Heap: %06d] Connected to address %s", freeheap, stdBLEAddress.c_str());
  FileSharingRemoteService = FileSharingClient->getService( FileSharingServiceUUID );
  if (FileSharingRemoteService == nullptr) {
    log_e("Failed to find our FileSharingServiceUUID: %s", FileSharingServiceUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bounding failed :-(");
    fileSharingServerIsRunning = false;
    vTaskDelete( NULL );
    return;
  }
  FileSharingReadRemoteChar = FileSharingRemoteService->getCharacteristic( FileSharingWriteUUID );
  if (FileSharingReadRemoteChar == nullptr) {
    log_e("Failed to find our characteristic FileSharingWriteUUID: %s, disconnecting", FileSharingWriteUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bad char. :-(");
    fileSharingServerIsRunning = false;
    vTaskDelete( NULL );
    return;
  }

  FileSharingRouterRemoteChar = FileSharingRemoteService->getCharacteristic( FileSharingRouteUUID );
  if (FileSharingRouterRemoteChar == nullptr) {
    log_e("Failed to find our characteristic FileSharingRouteUUID: %s, disconnecting", FileSharingRouteUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bad char. :-(");
    fileSharingServerIsRunning = false;
    vTaskDelete( NULL );
    return;
  }
  FileSharingRouterRemoteChar->registerForNotify( FileSharingRouterCallbacks );

  UI.headerStats("Connected :-)");

  const char* BLEFileToSend = "/" BLE_VENDOR_NAMES_DB_FILE;
  const char* MACFileToSend = "/" MAC_OUI_NAMES_DB_FILE;

  if( FileSharingRouterRemoteChar->writeValue((uint8_t*)BLEFileToSend, strlen(BLEFileToSend), true) ) {
    UI.headerStats("Discussing :-)");
    log_v("Will start sending %s file", BLEFileToSend );
    FileSharingSendFile( BLEFileToSend );
    if( !fileSharingSendFileError && FileSharingRouterRemoteChar->writeValue((uint8_t*)closeMessage, strlen(closeMessage), true) ) {
      log_v("Successfully sent bytes from %s file", BLEFileToSend );
      UI.headerStats("Copy complete :-)");
    } else {
      log_e("COPY ERROR FOR %s file", BLEFileToSend );
      UI.headerStats("Copy error :-(");
    }
  }

  if( !fileSharingSendFileError && FileSharingRouterRemoteChar->writeValue((uint8_t*)MACFileToSend, strlen(MACFileToSend), true) ) {
    UI.headerStats("Discussing :-)");
    log_v("Will start sending %s file", MACFileToSend );
    FileSharingSendFile( MACFileToSend );
    if( !fileSharingSendFileError && FileSharingRouterRemoteChar->writeValue((uint8_t*)closeMessage, strlen(closeMessage), true) ) {
      log_v("Successfully sent bytes from %s file", MACFileToSend );
      UI.headerStats("Copy complete :-)");
    } else {
      log_e("COPY ERROR FOR %s file", MACFileToSend );
      UI.headerStats("Copy error :-(");
    }
  } else {
    log_e("Skipping %s because previous errors", MACFileToSend );
  }

  if( FileSharingClient->isConnected() ) {
    FileSharingClient->disconnect();
  }

  fileSharingServerIsRunning = false;
  log_v("Deleting FileSharingClientTask" );
  vTaskDelete( NULL );

} // FileSharingClientTask
