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
bool bleAdvertising = false;
String commandBuffer = "";
bool processingCommand = false;
unsigned long commandStartTime = 0;
const unsigned long COMMAND_TIMEOUT = 30000; // 30 second timeout

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA8A"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA8A"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA8A"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      bleAdvertising = false;
      sprintf(serialBuffer, "BLE client connected\n");
      Serial.print(serialBuffer);
    };

    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      bleAdvertising = false;
      sprintf(serialBuffer, "BLE client disconnected\n");
      Serial.print(serialBuffer);
      
      // Clear any pending commands
      commandBuffer = "";
      processingCommand = false;
      
      // Restart advertising after a short delay
      delay(500);
      if (bleInitialized && pServer != NULL) {
        pServer->startAdvertising();
        bleAdvertising = true;
        sprintf(serialBuffer, "BLE advertising restarted\n");
        Serial.print(serialBuffer);
      }
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        commandBuffer += rxValue;
        
        sprintf(serialBuffer, "Received command: %s\n", rxValue.c_str());
        Serial.print(serialBuffer);
      }
    }
};

void connectionSetup() {
    sprintf(serialBuffer, "Starting BLE...\n");
    Serial.print(serialBuffer);
    
    BLEDevice::deinit(false);
    delay(100);
    
    BLEDevice::init("BrainBallast2");
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    
    // Set optimal BLE parameters for high throughput
    BLEDevice::setMTU(517);  // Maximum BLE MTU
    
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
    bleAdvertising = true;
    sprintf(serialBuffer, "BLE initialized with optimized settings\n");
    Serial.print(serialBuffer);
}

bool btIsConnected() {
    // Check both our flag AND the actual connection count
    if (!bleInitialized || pServer == NULL) {
        return false;
    }
    
    // BLEServer can report connected count
    bool actuallyConnected = (pServer->getConnectedCount() > 0);
    
    // If mismatch detected, fix it
    if (bleConnected != actuallyConnected) {
        sprintf(serialBuffer, "Connection state mismatch detected! bleConnected=%d, actual=%d\n", 
                bleConnected, actuallyConnected);
        Serial.print(serialBuffer);
        bleConnected = actuallyConnected;
        
        // If actually disconnected, restart advertising
        if (!actuallyConnected && !bleAdvertising) {
            pServer->startAdvertising();
            bleAdvertising = true;
            sprintf(serialBuffer, "Restarted advertising after mismatch\n");
            Serial.print(serialBuffer);
        }
    }
    
    return bleConnected && actuallyConnected;
}

bool btSendData(const char* data) {
    if (!btIsConnected() || processingCommand) {
        return false;
    }
    
    int dataLen = strlen(data);
    
    // Don't send empty data or single newlines
    if (dataLen == 0 || (dataLen == 1 && data[0] == '\n')) {
        return false;
    }
    
    const int maxChunk = 512;
    
    try {
        for (int i = 0; i < dataLen; i += maxChunk) {
            int chunkLen = min(maxChunk, dataLen - i);
            String chunk = String(data).substring(i, i + chunkLen);
            
            pTxCharacteristic->setValue(chunk.c_str());
            pTxCharacteristic->notify();
            
            // Reduced delay for better speed
            if (chunkLen == maxChunk && dataLen > 1024) {
                delayMicroseconds(500);
            }
        }
        
        sprintf(serialBuffer, "BLE sent %d bytes (sensor data)\n", dataLen);
        Serial.print(serialBuffer);
        return true;
    } catch (...) {
        sprintf(serialBuffer, "BLE send failed - exception caught\n");
        Serial.print(serialBuffer);
        bleConnected = false;
        return false;
    }
}

void btReconnect() {
    if (!bleInitialized || pServer == NULL) {
        return;
    }
    
    // Only try to restart advertising if we're not connected AND not already advertising
    if (!bleConnected && !bleAdvertising) {
        sprintf(serialBuffer, "Starting BT reconnection...\n");
        Serial.print(serialBuffer);
        
        pServer->startAdvertising();
        bleAdvertising = true;
        sprintf(serialBuffer, "Restarted BLE advertising\n");
        Serial.print(serialBuffer);
    }
}

void btHandleCommands() {
    // Check for command timeout
    if (processingCommand) {
        if (millis() - commandStartTime > COMMAND_TIMEOUT) {
            sprintf(serialBuffer, "Command timeout! Clearing flag.\n");
            Serial.print(serialBuffer);
            processingCommand = false;
            commandBuffer = "";
        }
    }
    
    if (commandBuffer.length() > 0 && (commandBuffer.indexOf('\n') >= 0 || commandBuffer.indexOf('\r') >= 0)) {
        int newlineIndex = max(commandBuffer.indexOf('\n'), commandBuffer.indexOf('\r'));
        String command = commandBuffer.substring(0, newlineIndex);
        commandBuffer = commandBuffer.substring(newlineIndex + 1);
        
        command.trim();
        if (command.length() > 0) {
            processingCommand = true;
            commandStartTime = millis();
            processCommand(command);
            processingCommand = false;
        }
    }
}

void processCommand(String command) {
    sprintf(serialBuffer, "Processing command: %s\n", command.c_str());
    Serial.print(serialBuffer);
    
    // Send acknowledgment
    String ack = "ACK: " + command + "\n";
    sendCommandResponse(ack.c_str());
    
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
            if (amount > 0 && amount <= 1000) {
                storageTailFile(filename, amount);
            } else {
                sendCommandResponse("ERROR: Invalid line count (1-1000)\n");
            }
        } else {
            sendCommandResponse("ERROR: Usage: tail <filename> <lines>\n");
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
    else if (command.equals("ping")) {
        sendCommandResponse("PONG\n");
    }
    else {
        sprintf(serialBuffer, "Ignoring unknown command: %s\n", command.c_str());
        Serial.print(serialBuffer);
    }
}

void sendCommandResponse(const char* response) {
    if (!btIsConnected()) {
        return;
    }
    
    int dataLen = strlen(response);
    const int maxChunk = 512;
    
    try {
        for (int i = 0; i < dataLen; i += maxChunk) {
            int chunkLen = min(maxChunk, dataLen - i);
            String chunk = String(response).substring(i, i + chunkLen);
            
            pTxCharacteristic->setValue(chunk.c_str());
            pTxCharacteristic->notify();
            
            if (chunkLen == maxChunk) {
                delayMicroseconds(500);
            }
        }
    } catch (...) {
        sprintf(serialBuffer, "Command response send failed\n");
        Serial.print(serialBuffer);
    }
}