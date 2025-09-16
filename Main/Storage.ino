#include <SPI.h>
#include <SD.h>

const int SD_CS_PIN   = D7;
const int SD_MOSI_PIN = D10;
const int SD_MISO_PIN = D9;
const int SD_SCK_PIN  = D8;

// Change to "/logs/data.txt" if you want a logs folder
// (also call SD.mkdir("/logs") once in storageSetup)
const char* LOG_FILENAME = "data.txt";

bool sdCardInitialized = false;

extern char serialBuffer[128];

void storageSetup() {
    sprintf(serialBuffer, "Starting Storage...\n");
    Serial.print(serialBuffer);
    
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    delay(100);
    
    if (SD.begin(SD_CS_PIN, SPI, 400000)) {   // 400 kHz safer for init
        sdCardInitialized = true;
        sprintf(serialBuffer, "SD card initialized\n");
        Serial.print(serialBuffer);

        // Optional: create logs folder if using "/logs/data.txt"
        // SD.mkdir("/logs");
    } else {
        sprintf(serialBuffer, "SD card init failed - continuing without SD\n");
        Serial.print(serialBuffer);
    }
}

bool storageWrite(const char* data) {
    if (!sdCardInitialized) {
        if (!storageInit()) {
            return false;
        }
    }

    File dataFile = SD.open(LOG_FILENAME, FILE_WRITE);

    if (!dataFile) {
        Serial.println("⚠️ SD.open failed, retrying init...");
        if (storageInit()) {
            dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
        }
    }

    if (dataFile) {
        size_t len = strlen(data);
        const size_t chunkSize = 128;

        for (size_t i = 0; i < len; i += chunkSize) {
            size_t n = min(chunkSize, len - i);
            dataFile.write((const uint8_t*)(data + i), n);
        }

        dataFile.close();  // flush + close
        sprintf(serialBuffer, "Data written to SD (%d bytes)\n", (int)len);
        Serial.print(serialBuffer);
        return true;
    } else {
        Serial.println("❌ SD write failed after re-init");
        return false;
    }
}

bool storageInit() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    delay(50);
    
    if (SD.begin(SD_CS_PIN, SPI, 400000)) {
        sdCardInitialized = true;
        sprintf(serialBuffer, "SD card re-initialized\n");
        Serial.print(serialBuffer);
        return true;
    } else {
        sprintf(serialBuffer, "SD card re-init failed\n");
        Serial.print(serialBuffer);
        return false;
    }
}

bool storageWriteBatch(const char* lines[], int count) {
    if (!sdCardInitialized) {
        if (!storageInit()) {
            return false;
        }
    }
    
    File dataFile = SD.open(LOG_FILENAME, FILE_WRITE);

    if (!dataFile) {
        Serial.println("⚠️ SD.open (batch) failed, retrying init...");
        if (storageInit()) {
            dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
        }
    }

    if (dataFile) {
        for (int i = 0; i < count; i++) {
            size_t len = strlen(lines[i]);
            const size_t chunkSize = 128;

            for (size_t j = 0; j < len; j += chunkSize) {
                size_t n = min(chunkSize, len - j);
                dataFile.write((const uint8_t*)(lines[i] + j), n);
            }
        }

        dataFile.close();
        sprintf(serialBuffer, "Batch of %d lines written to SD\n", count);
        Serial.print(serialBuffer);
        return true;
    } else {
        Serial.println("❌ SD batch write failed after re-init");
        return false;
    }
}
