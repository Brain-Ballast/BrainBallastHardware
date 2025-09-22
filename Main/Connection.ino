#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
BLECharacteristic* pRxCharacteristic;
extern char serialBuffer[128];

bool bleConnected = false;
bool bleInitialized = false;
String commandBuffer = "";

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

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        commandBuffer += rxValue;
        
        sprintf(serialBuffer, "Received: %s\n", rxValue.c_str());
        Serial.print(serialBuffer);
      }
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

    pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_WRITE
                     );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

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
        
        delay(10);
    }
    
    sprintf(serialBuffer, "BLE sent %d bytes\n", dataLen);
    Serial.print(serialBuffer);
    return true;
}

void btReconnect() {
    if (!bleInitialized) return;
    
    unsigned long startReconnect = millis();
    sprintf(serialBuffer, "Starting BT reconnection...\n");
    Serial.print(serialBuffer);
    
    // Quick reconnect attempt - timeout after 5 seconds max
    if (!bleConnected && bleInitialized) {
        pServer->startAdvertising();
        sprintf(serialBuffer, "Restarted BLE advertising\n");
        Serial.print(serialBuffer);
        
        // Don't block - just restart advertising and return quickly
        unsigned long elapsed = millis() - startReconnect;
        sprintf(serialBuffer, "BT reconnect attempt completed in %lums\n", elapsed);
        Serial.print(serialBuffer);
    }
}

void btHandleCommands() {
    if (commandBuffer.length() > 0 && (commandBuffer.indexOf('\n') >= 0 || commandBuffer.indexOf('\r') >= 0)) {
        int newlineIndex = max(commandBuffer.indexOf('\n'), commandBuffer.indexOf('\r'));
        String command = commandBuffer.substring(0, newlineIndex);
        commandBuffer = commandBuffer.substring(newlineIndex + 1);
        
        command.trim();
        if (command.length() > 0) {
            processCommand(command);
        }
    }
}

void processCommand(String command) {
    sprintf(serialBuffer, "Processing command: %s\n", command.c_str());
    Serial.print(serialBuffer);
    
    // Send acknowledgment that we received the command
    String ack = "Received: " + command + "\n";
    btSendData(ack.c_str());
    
    if (command.equals("list")) {
        storageListFiles();
    }
    else if (command.startsWith("download ")) {
        String filename = command.substring(9);
        filename.trim();
        storageDownloadFile(filename);
    }
    else if (command.startsWith("tail ")) {
        int spaceIndex = command.lastIndexOf(' ');
        if (spaceIndex > 5) {
            String filename = command.substring(5, spaceIndex);
            String amountStr = command.substring(spaceIndex + 1);
            filename.trim();
            int amount = amountStr.toInt();
            if (amount > 0) {
                storageTailFile(filename, amount);
            } else {
                btSendData("Invalid line count for tail command\n");
            }
        } else {
            btSendData("Usage: tail <filename> <lines>\n");
        }
    }
    else if (command.startsWith("size ")) {
        String filename = command.substring(5);
        filename.trim();
        storageFileSize(filename);
    }
    else if (command.startsWith("delete ")) {
        String filename = command.substring(7);
        filename.trim();
        storageDeleteFile(filename);
    }
    else if (command.equals("info")) {
        storageInfo();
    }
    else if (command.equals("test")) {
        btSendData("Test response: Arduino is responding to commands!\n");
    }
    else {
        String response = "Unknown command: " + command + "\n";
        btSendData(response.c_str());
    }
}

void sendHelp() {
    // Help is now handled on Python side - this function removed
}