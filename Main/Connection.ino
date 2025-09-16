#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
extern char serialBuffer[128];

bool bleConnected = false;
bool bleInitialized = false;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      sprintf(serialBuffer, "BLE client connected\n");
      Serial.print(serialBuffer);
    };

    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      sprintf(serialBuffer, "BLE client disconnected\n");
      Serial.print(serialBuffer);
      delay(500);
      pServer->startAdvertising();
    }
};

void connectionSetup() {
    sprintf(serialBuffer, "Starting BLE...\n");
    Serial.print(serialBuffer);
    
    BLEDevice::deinit(false);
    delay(100);
    
    BLEDevice::init("BrainBallast");
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
                      
    pTxCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();
    
    bleInitialized = true;
    sprintf(serialBuffer, "BLE initialized and advertising\n");
    Serial.print(serialBuffer);
}

bool btIsConnected() {
    return bleConnected;
}

bool btSendData(const char* data) {
    if (!bleConnected || !bleInitialized) {
        return false;
    }
    
    int dataLen = strlen(data);
    const int maxChunk = 200;
    
    for (int i = 0; i < dataLen; i += maxChunk) {
        int chunkLen = min(maxChunk, dataLen - i);
        String chunk = String(data).substring(i, i + chunkLen);
        
        pTxCharacteristic->setValue(chunk.c_str());
        pTxCharacteristic->notify();
        
        delay(50);
    }
    
    sprintf(serialBuffer, "BLE sent %d bytes\n", dataLen);
    Serial.print(serialBuffer);
    return true;
}
