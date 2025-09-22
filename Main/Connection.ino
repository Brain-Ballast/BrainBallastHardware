// Add to Connection.ino - Command-based SD card access via BT

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
BLECharacteristic* pRxCharacteristic; // Add RX for commands
extern char serialBuffer[128];

bool bleConnected = false;
bool bleInitialized = false;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Command processing
struct BtCommand {
    String command;
    String param1;
    String param2;
};

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
      String rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        sprintf(serialBuffer, "BT Command received: %s\n", rxValue.c_str());
        Serial.print(serialBuffer);
        
        processBtCommand(rxValue);
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

    // TX Characteristic (ESP32 -> Client)
    pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic (Client -> ESP32)
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
        
        delay(50);
    }
    
    sprintf(serialBuffer, "BLE sent %d bytes\n", dataLen);
    Serial.print(serialBuffer);
    return true;
}

BtCommand parseBtCommand(String input) {
    BtCommand cmd;
    input.trim();
    
    int firstSpace = input.indexOf(' ');
    if (firstSpace == -1) {
        cmd.command = input;
        return cmd;
    }
    
    cmd.command = input.substring(0, firstSpace);
    String remainder = input.substring(firstSpace + 1);
    
    int secondSpace = remainder.indexOf(' ');
    if (secondSpace == -1) {
        cmd.param1 = remainder;
        return cmd;
    }
    
    cmd.param1 = remainder.substring(0, secondSpace);
    cmd.param2 = remainder.substring(secondSpace + 1);
    
    return cmd;
}

void processBtCommand(String input) {
    BtCommand cmd = parseBtCommand(input);
    
    if (cmd.command == "list" || cmd.command == "LS") {
        handleListFiles();
    }
    else if (cmd.command == "download" && cmd.param1.length() > 0) {
        handleGetFile(cmd.param1);
    }
    else if (cmd.command == "tail" && cmd.param1.length() > 0) {
        int lines = cmd.param2.length() > 0 ? cmd.param2.toInt() : 10;
        handleGetTail(cmd.param1, lines);
    }
    else if (cmd.command == "size" && cmd.param1.length() > 0) {
        handleGetFileSize(cmd.param1);
    }
    else if (cmd.command == "delete" && cmd.param1.length() > 0) {
        handleDeleteFile(cmd.param1);
    }
    else if (cmd.command == "info") {
        handleSdInfo();
    }
    else if (cmd.command == "help") {
        handleHelp();
    }
    else {
        btSendResponse("ERROR: Unknown command. Send 'help' for available commands.");
    }
}

void btSendResponse(const char* response) {
    if (btSendData(response)) {
        // Success
    } else {
        sprintf(serialBuffer, "Failed to send BT response\n");
        Serial.print(serialBuffer);
    }
}

void handleListFiles() {
    sprintf(serialBuffer, "BT: Listing SD files\n");
    Serial.print(serialBuffer);
    
    String response = "FILES:\n";
    
    File root = SD.open("/");
    if (!root) {
        btSendResponse("ERROR: Cannot access SD card");
        return;
    }
    
    if (!root.isDirectory()) {
        btSendResponse("ERROR: Root is not a directory");
        root.close();
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            response += String(file.name()) + " (" + String(file.size()) + " bytes)\n";
        }
        file = root.openNextFile();
    }
    root.close();
    
    response += "END_FILES\n";
    btSendResponse(response.c_str());
}

void handleGetFile(String filename) {
    sprintf(serialBuffer, "BT: Getting file %s\n", filename.c_str());
    Serial.print(serialBuffer);
    
    // Add leading slash if not present
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    File file = SD.open(filename);
    if (!file) {
        btSendResponse("ERROR: File not found");
        return;
    }
    
    // Send file header
    String header = "FILE_START:" + filename + ":" + String(file.size()) + "\n";
    btSendResponse(header.c_str());
    
    // Send file content in chunks
    const int bufferSize = 500;
    char buffer[bufferSize];
    
    while (file.available()) {
        int bytesRead = file.readBytes(buffer, bufferSize - 1);
        buffer[bytesRead] = '\0';
        
        if (!btSendData(buffer)) {
            sprintf(serialBuffer, "BT send failed during file transfer\n");
            Serial.print(serialBuffer);
            break;
        }
    }
    
    file.close();
    btSendResponse("FILE_END\n");
}

void handleGetTail(String filename, int lines) {
    sprintf(serialBuffer, "BT: Getting last %d lines from %s\n", lines, filename.c_str());
    Serial.print(serialBuffer);
    
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    File file = SD.open(filename);
    if (!file) {
        btSendResponse("ERROR: File not found");
        return;
    }
    
    // For simplicity, read entire file and send last N lines
    // (For large files, you'd want to implement backwards reading)
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
    // Split into lines and get last N
    int lineCount = 0;
    int lastNewlines[lines + 1];
    lastNewlines[0] = content.length(); // End of file
    
    // Find positions of last N newlines
    for (int i = content.length() - 1; i >= 0 && lineCount < lines; i--) {
        if (content[i] == '\n') {
            lineCount++;
            lastNewlines[lineCount] = i;
        }
    }
    
    // Send the last lines
    String response = "TAIL_START:" + filename + ":" + String(lineCount) + "\n";
    
    int startPos = (lineCount == lines) ? lastNewlines[lines] + 1 : 0;
    response += content.substring(startPos);
    
    response += "TAIL_END\n";
    btSendResponse(response.c_str());
}

void handleGetFileSize(String filename) {
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    File file = SD.open(filename);
    if (!file) {
        btSendResponse("ERROR: File not found");
        return;
    }
    
    String response = "SIZE:" + filename + ":" + String(file.size()) + " bytes\n";
    file.close();
    btSendResponse(response.c_str());
}

void handleDeleteFile(String filename) {
    sprintf(serialBuffer, "BT: Deleting file %s\n", filename.c_str());
    Serial.print(serialBuffer);
    
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    if (SD.remove(filename)) {
        btSendResponse("SUCCESS: File deleted\n");
    } else {
        btSendResponse("ERROR: Failed to delete file\n");
    }
}

void handleSdInfo() {
    String response = "SD_INFO:\n";
    response += "Total: " + String(SD.totalBytes() / (1024 * 1024)) + "MB\n";
    response += "Used: " + String(SD.usedBytes() / (1024 * 1024)) + "MB\n";
    response += "Free: " + String((SD.totalBytes() - SD.usedBytes()) / (1024 * 1024)) + "MB\n";
    
    uint8_t cardType = SD.cardType();
    response += "Type: ";
    if (cardType == CARD_MMC) response += "MMC\n";
    else if (cardType == CARD_SD) response += "SDSC\n";
    else if (cardType == CARD_SDHC) response += "SDHC\n";
    else response += "UNKNOWN\n";
    
    btSendResponse(response.c_str());
}

void handleHelp() {
    String help = "COMMANDS:\n";
    help += "list or LS - List all files\n";
    help += "download filename - Download entire file\n";
    help += "tail filename [lines] - Get last N lines (default 10)\n";
    help += "size filename - Get file size\n";
    help += "delete filename - Delete file\n";
    help += "info - Show SD card info\n";
    help += "help - Show this help\n";
    
    btSendResponse(help.c_str());
}

void btReconnect() {
    // Previous reconnect code remains the same...
    unsigned long startTime = millis();
    const unsigned long maxReconnectTime = 5000;
    
    sprintf(serialBuffer, "BT reconnection attempt starting...\n");
    Serial.print(serialBuffer);
    
    if (bleInitialized && pServer != NULL) {
        pServer->getAdvertising()->stop();
        delay(100);
        
        if (millis() - startTime > maxReconnectTime) {
            sprintf(serialBuffer, "BT reconnect timeout (stop advertising)\n");
            Serial.print(serialBuffer);
            return;
        }
        
        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMinPreferred(0x12);
        
        if (millis() - startTime > maxReconnectTime) {
            sprintf(serialBuffer, "BT reconnect timeout (before restart)\n");
            Serial.print(serialBuffer);
            return;
        }
        
        pAdvertising->start();
        
        sprintf(serialBuffer, "BLE advertising restarted (%lu ms)\n", millis() - startTime);
        Serial.print(serialBuffer);
    }
    
    unsigned long totalTime = millis() - startTime;
    sprintf(serialBuffer, "BT reconnect attempt completed in %lu ms\n", totalTime);
    Serial.print(serialBuffer);
}