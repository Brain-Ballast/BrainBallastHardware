#define SERIAL_BAUD 115200

// Global variable definitions
char serialBuffer[128];
char outputBuffer[32768];  // Increased buffer size
char storageBuffer[32768]; // Increased buffer size
char lastCSVLine[128];     // Store last sensor reading for printing

unsigned long lastSensorReading = 0;
unsigned long lastBTTransmit = 0;
unsigned long lastStorageWrite = 0;
unsigned long lastBTReconnect = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastSerialPrint = 0;
unsigned long startTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    startTime = millis();
    
    // Initialize buffers
    outputBuffer[0] = '\0';
    storageBuffer[0] = '\0';
    lastCSVLine[0] = '\0';
    
    pressureSetup();
    adxlSetup();
    storageSetup();
    connectionSetup();
    
    // Add CSV header
    sprintf(serialBuffer, "pres,temp,x,y,z,timestamp\n");
    Serial.print("CSV Header: ");
    Serial.print(serialBuffer);
}

void loop() {
    // Read BOTH sensors every 20ms (50Hz)
    if (checkTimer(lastSensorReading, 20)) {
        readSensorsToCSV();
    }
    
    // Check commands less frequently
    if (checkTimer(lastCommandCheck, 100)) {
        btHandleCommands();
    }
    
    // Send to BT every 10 seconds
    if (checkTimer(lastBTTransmit, 10000)) {
        if (strlen(outputBuffer) > 0) {
            if (btSendData(outputBuffer)) {
                // Successfully sent, clear buffer
                outputBuffer[0] = '\0';
            } else {
                // BT not connected, keep data cached
                sprintf(serialBuffer, "BT not connected, data cached (%d chars)\n", strlen(outputBuffer));
                Serial.print(serialBuffer);
            }
        }
    }

    // Write to SD card every 30 seconds
    if (checkTimer(lastStorageWrite, 30000)) {
        if (strlen(storageBuffer) > 0) {
            sprintf(serialBuffer, "Writing to storage (%d chars)\n", strlen(storageBuffer));
            Serial.print(serialBuffer);
            storageWrite(storageBuffer);
            storageBuffer[0] = '\0';
        }
    }
    
    // Try to reconnect every 2 minutes if not connected
    if (checkTimer(lastBTReconnect, 120000)) {
        if (!btIsConnected()) {
            btReconnect();
        }
    }
    
    // Print status every 500ms
    if (checkTimer(lastSerialPrint, 500)) {
        printStatus();
    }
}

bool checkTimer(unsigned long &lastTrigger, unsigned long interval) {
    unsigned long now = millis();
    if (now - lastTrigger >= interval) {
        lastTrigger = now;
        return true;
    }
    return false;
}

void readSensorsToCSV() {
    static int sensorCount = 0;
    
    // Read both sensors
    float pressure, temperature;
    pressureRead(&pressure, &temperature);
    
    float x_g, y_g, z_g;
    adxlRead(&x_g, &y_g, &z_g);
    
    unsigned long timestamp = millis();
    
    // Create CSV line and store it globally for printing
    sprintf(lastCSVLine, "%.2f,%.2f,%.2f,%.2f,%.2f,%lu\n", 
            pressure, temperature, x_g, y_g, z_g, timestamp);
    
    // Print every 25th reading (25 * 20ms = 500ms)
    sensorCount++;
    if (sensorCount % 25 == 0) {
        Serial.print(lastCSVLine);
    }
    
    // Only add to BT buffer if connected
    if (btIsConnected()) {
        if (strlen(outputBuffer) + strlen(lastCSVLine) < sizeof(outputBuffer) - 1) {
            strcat(outputBuffer, lastCSVLine);
        } else {
            sprintf(serialBuffer, "WARNING: BT buffer full, clearing old data\n");
            Serial.print(serialBuffer);
            outputBuffer[0] = '\0';
            strcat(outputBuffer, lastCSVLine);
        }
    }
    
    // ALWAYS add to storage buffer (regardless of BT connection)
    if (strlen(storageBuffer) + strlen(lastCSVLine) < sizeof(storageBuffer) - 1) {
        strcat(storageBuffer, lastCSVLine);
    } else {
        sprintf(serialBuffer, "WARNING: Storage buffer full, clearing old data\n");
        Serial.print(serialBuffer);
        storageBuffer[0] = '\0';
        strcat(storageBuffer, lastCSVLine);
    }
}

void printStatus() {
    static int printCount = 0;
    printCount++;
    
    // Print buffer status every 500ms
    sprintf(serialBuffer, "[Status] BT=%d chars, Storage=%d chars, Connected=%s\n", 
            (int)strlen(outputBuffer), (int)strlen(storageBuffer), 
            btIsConnected() ? "YES" : "NO");
    Serial.print(serialBuffer);
}