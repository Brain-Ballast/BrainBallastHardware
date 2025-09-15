#include <SPI.h>
#include <SD.h>

// SD Card Configuration for XIAO ESP32-C3
const int SD_CS_PIN = D7;
const char* LOG_FILENAME = "data.txt";

// Storage state
bool sdCardInitialized = false;
bool sdCardSleeping = false;

extern char serialBuffer[128];

void storageSetup() {
    sprintf(serialBuffer, "Starting Storage...\n");
    Serial.print(serialBuffer);
    
    // Initialize SPI pins
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    if (SD.begin(SD_CS_PIN)) {
        sdCardInitialized = true;
        sprintf(serialBuffer, "SD card initialized\n");
        Serial.print(serialBuffer);
    } else {
        sprintf(serialBuffer, "SD card init failed\n");
        Serial.print(serialBuffer);
    }
    sprintf(serialBuffer, "Storage initialized\n");
    Serial.print(serialBuffer);
}

bool storageWrite(const char* data) {
    bool success = false;

    if (sdCardSleeping) {
        storageWake();
    }

    if (!sdCardInitialized) {
        if (!storageInit()) {
            sprintf(serialBuffer, "SD init failed\n");
            Serial.print(serialBuffer);
            storageSleep(); // Put back to sleep on failure
            return false;
        }
    }
    
    File dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (dataFile) {
        dataFile.print(data);
        dataFile.close();
        success = true;
        
        sprintf(serialBuffer, "Data written to SD (%d bytes)\n", strlen(data));
        Serial.print(serialBuffer);
    } else {
        sprintf(serialBuffer, "Error opening %s\n", LOG_FILENAME);
        Serial.print(serialBuffer);
    }
  
    storageSleep();
    
    return success;
}

bool storageInit() {
    if (SD.begin(SD_CS_PIN)) {
        sdCardInitialized = true;
        sprintf(serialBuffer, "SD card re-initialized\n");
        Serial.print(serialBuffer);
        return true;
    } else {
        sprintf(serialBuffer, "SD card init failed\n");
        Serial.print(serialBuffer);
        return false;
    }
}

void storageSleep() {
    if (!sdCardSleeping) {
        // Method 1: SPI Bus sleep
        digitalWrite(SD_CS_PIN, HIGH);
        SPI.end();
        
        sdCardSleeping = true;
        sdCardInitialized = false;
        
        sprintf(serialBuffer, "SD card sleeping\n");
        Serial.print(serialBuffer);
    }
}

void storageWake() {
    if (sdCardSleeping) {
        SPI.begin();
        delay(10);
        sdCardSleeping = false;
        sprintf(serialBuffer, "SD card waking\n");
        Serial.print(serialBuffer);
    }
}

bool storageWriteWithTimestamp(const char* data) {
    char timestampBuffer[2200];
    sprintf(timestampBuffer, "%lu,%s", millis(), data);
    return storageWrite(timestampBuffer);
}

bool storageWriteBatch(const char* lines[], int count) {
    bool success = false;
    if (sdCardSleeping) {
        storageWake();
    }
    
    if (!sdCardInitialized) {
        if (!storageInit()) {
            sprintf(serialBuffer, "SD init failed\n");
            Serial.print(serialBuffer);
            storageSleep();
            return false;
        }
    }
    
    File dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (dataFile) {
        for (int i = 0; i < count; i++) {
            dataFile.print(lines[i]);
        }
        dataFile.close();
        success = true;
        
        sprintf(serialBuffer, "Batch of %d lines written to SD\n", count);
        Serial.print(serialBuffer);
    } else {
        sprintf(serialBuffer, "Error opening %s for batch write\n", LOG_FILENAME);
        Serial.print(serialBuffer);
    }
    
    storageSleep();
    
    return success;
}

void storageSleepNow() {
    storageSleep();
}

void storageWakeNow() {
    storageWake();
}