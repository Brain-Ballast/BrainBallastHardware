#include "FS.h"
#include "SD.h"
#include "SPI.h"

// DO NOT define REASSIGN_PINS - use default pins like the working example
// The working example shows that ESP32-C3 default pins are:
// CS (SS): GPIO7, MOSI: GPIO6, MISO: GPIO5, SCK: GPIO4

const char* LOG_FILENAME = "/data.txt";
bool sdCardInitialized = false;

// External reference to serialBuffer (defined in main)
extern char serialBuffer[128];

void storageSetup() {
    sprintf(serialBuffer, "Starting Storage...\n");
    Serial.print(serialBuffer);
    
    // Use EXACTLY the same initialization as working example - NO custom pins
    if (!SD.begin()) {
        sprintf(serialBuffer, "Card Mount Failed - continuing without SD\n");
        Serial.print(serialBuffer);
        sdCardInitialized = false;
        return;
    }
    
    // Same card detection as working example
    uint8_t cardType = SD.cardType();
    
    if (cardType == CARD_NONE) {
        sprintf(serialBuffer, "No SD card attached\n");
        Serial.print(serialBuffer);
        sdCardInitialized = false;
        return;
    }
    
    sdCardInitialized = true;
    sprintf(serialBuffer, "SD card initialized successfully\n");
    Serial.print(serialBuffer);
    
    // Print card info exactly like working example
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    // Test basic functionality
    sprintf(serialBuffer, "Testing SD card operations...\n");
    Serial.print(serialBuffer);
    
    // Test directory listing like working example
    Serial.printf("Listing directory: %s\n", "/");
    File root = SD.open("/");
    if (root) {
        if (root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (file.isDirectory()) {
                    Serial.print("  DIR : ");
                    Serial.println(file.name());
                } else {
                    Serial.print("  FILE: ");
                    Serial.print(file.name());
                    Serial.print("  SIZE: ");
                    Serial.println(file.size());
                }
                file = root.openNextFile();
            }
        }
        root.close();
    }
    
    // Simple test write - exactly like working example
    Serial.printf("Writing file: %s\n", "/test_init.txt");
    File testFile = SD.open("/test_init.txt", FILE_WRITE);
    if (!testFile) {
        Serial.println("Failed to open test file for writing");
    } else {
        if (testFile.print("SD card test - initialization successful\n")) {
            Serial.println("Test file written");
        } else {
            Serial.println("Test write failed");
        }
        testFile.close();
        
        // Read it back
        Serial.printf("Reading file: %s\n", "/test_init.txt");
        File readTest = SD.open("/test_init.txt");
        if (readTest) {
            Serial.print("Read from file: ");
            while (readTest.available()) {
                Serial.write(readTest.read());
            }
            readTest.close();
        }
        
        // Clean up
        SD.remove("/test_init.txt");
    }
    
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

bool storageWrite(const char* data) {
    if (!sdCardInitialized) {
        sprintf(serialBuffer, "SD card not initialized, skipping write\n");
        Serial.print(serialBuffer);
        return false;
    }
    
    // Simple append operation like working example
    Serial.printf("Appending to file: %s\n", LOG_FILENAME);
    
    File dataFile = SD.open(LOG_FILENAME, FILE_APPEND);
    if (!dataFile) {
        sprintf(serialBuffer, "Failed to open file for appending\n");
        Serial.print(serialBuffer);
        return false;
    }
    
    if (dataFile.print(data)) {
        sprintf(serialBuffer, "Message appended (%d bytes)\n", (int)strlen(data));
        Serial.print(serialBuffer);
        dataFile.close();
        return true;
    } else {
        sprintf(serialBuffer, "Append failed\n");
        Serial.print(serialBuffer);
        dataFile.close();
        return false;
    }
}

bool storageInit() {
    // Reinitialize using same method as setup - NO custom pins
    if (!SD.begin()) {
        sprintf(serialBuffer, "SD card re-init failed\n");
        Serial.print(serialBuffer);
        sdCardInitialized = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        sprintf(serialBuffer, "No SD card attached after re-init\n");
        Serial.print(serialBuffer);
        sdCardInitialized = false;
        return false;
    }
    
    sdCardInitialized = true;
    sprintf(serialBuffer, "SD card re-initialized successfully\n");
    Serial.print(serialBuffer);
    return true;
}

bool storageWriteBatch(const char* lines[], int count) {
    if (!sdCardInitialized) {
        sprintf(serialBuffer, "SD card not initialized, skipping batch write\n");
        Serial.print(serialBuffer);
        return false;
    }
    
    // Open file once for batch write
    File dataFile = SD.open(LOG_FILENAME, FILE_APPEND);
    
    if (!dataFile) {
        sprintf(serialBuffer, "Failed to open file for batch writing\n");
        Serial.print(serialBuffer);
        return false;
    }
    
    for (int i = 0; i < count; i++) {
        dataFile.print(lines[i]);
    }
    
    dataFile.close();
    sprintf(serialBuffer, "Batch of %d lines written to SD\n", count);
    Serial.print(serialBuffer);
    return true;
}

// Command handler functions for BT interface
void storageListFiles() {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    btSendData("Files on SD card:\n");
    File root = SD.open("/");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        bool foundFiles = false;
        while (file) {
            if (!file.isDirectory()) {
                String fileInfo = String(file.name()) + " (" + String(file.size()) + " bytes)\n";
                btSendData(fileInfo.c_str());
                foundFiles = true;
            }
            file = root.openNextFile();
        }
        root.close();
        
        if (!foundFiles) {
            btSendData("No files found\n");
        }
    } else {
        btSendData("Failed to open root directory\n");
    }
    btSendData("End of file list\n");
}

void storageDownloadFile(String filename) {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    String filePath = "/" + filename;
    File file = SD.open(filePath.c_str());
    
    if (!file) {
        String response = "File not found: " + filename + "\n";
        btSendData(response.c_str());
        return;
    }
    
    // Send file info first
    String response = "=== Downloading " + filename + " (" + String(file.size()) + " bytes) ===\n";
    btSendData(response.c_str());
    
    // Read and send file in larger chunks optimized for ESP32-C3 BLE
    char buffer[512];  // Larger buffer for better throughput
    int totalSent = 0;
    while (file.available()) {
        int bytesRead = file.read((uint8_t*)buffer, sizeof(buffer) - 1);
        buffer[bytesRead] = '\0';
        
        // Send data immediately without delay for max speed
        if (btSendData(buffer)) {
            totalSent += bytesRead;
        } else {
            // Only add delay if send failed (BT buffer full)
            delay(50);
        }
    }
    
    file.close();
    btSendData("\n=== End of file ===\n");
    
    sprintf(serialBuffer, "Download complete: %d bytes sent\n", totalSent);
    Serial.print(serialBuffer);
}

void storageTailFile(String filename, int lines) {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    String filePath = "/" + filename;
    File file = SD.open(filePath.c_str());
    
    if (!file) {
        String response = "File not found: " + filename + "\n";
        btSendData(response.c_str());
        return;
    }
    
    // Simple tail implementation - read entire file and return last N lines
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
    // Count lines from the end
    int lineCount = 0;
    int pos = content.length() - 1;
    while (pos >= 0 && lineCount < lines) {
        if (content.charAt(pos) == '\n') {
            lineCount++;
        }
        pos--;
    }
    
    String result = content.substring(pos + 2); // +2 to skip the newline
    String response = "=== Last " + String(lines) + " lines of " + filename + " ===\n";
    btSendData(response.c_str());
    btSendData(result.c_str());
}

void storageFileSize(String filename) {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    String filePath = "/" + filename;
    File file = SD.open(filePath.c_str());
    
    if (!file) {
        String response = "File not found: " + filename + "\n";
        btSendData(response.c_str());
        return;
    }
    
    String response = filename + ": " + String(file.size()) + " bytes\n";
    file.close();
    btSendData(response.c_str());
}

void storageDeleteFile(String filename) {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    String filePath = "/" + filename;
    if (SD.remove(filePath.c_str())) {
        String response = "Deleted: " + filename + "\n";
        btSendData(response.c_str());
    } else {
        String response = "Failed to delete: " + filename + "\n";
        btSendData(response.c_str());
    }
}

void storageInfo() {
    if (!sdCardInitialized) {
        btSendData("SD card not initialized\n");
        return;
    }
    
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    uint64_t freeBytes = totalBytes - usedBytes;
    
    String info = "SD Card Info:\n";
    info += "Total: " + String((double)totalBytes / (1024 * 1024), 1) + " MB\n";
    info += "Used: " + String((double)usedBytes / (1024 * 1024), 1) + " MB\n";
    info += "Free: " + String((double)freeBytes / (1024 * 1024), 1) + " MB\n";
    info += "Usage: " + String((double)usedBytes / totalBytes * 100, 1) + "%\n";
    
    btSendData(info.c_str());
}

// Debug function to check what's happening
void storageDebug() {
    sprintf(serialBuffer, "\n=== SD Card Debug Info ===\n");
    Serial.print(serialBuffer);
    
    sprintf(serialBuffer, "Using default ESP32-C3 SPI pins (no custom assignment)\n");
    Serial.print(serialBuffer);
    
    sprintf(serialBuffer, "Default pins: CS=GPIO7, MOSI=GPIO6, MISO=GPIO5, SCK=GPIO4\n");
    Serial.print(serialBuffer);
    
    sprintf(serialBuffer, "SD initialized: %s\n", sdCardInitialized ? "YES" : "NO");
    Serial.print(serialBuffer);
    
    if (sdCardInitialized) {
        uint8_t cardType = SD.cardType();
        sprintf(serialBuffer, "Card type: %d\n", cardType);
        Serial.print(serialBuffer);
        
        sprintf(serialBuffer, "Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
        Serial.print(serialBuffer);
        
        sprintf(serialBuffer, "Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
        Serial.print(serialBuffer);
        
        // List current files
        Serial.printf("Current files:\n");
        File root = SD.open("/");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
                }
                file = root.openNextFile();
            }
            root.close();
        }
    }
    
    sprintf(serialBuffer, "=========================\n");
    Serial.print(serialBuffer);
}