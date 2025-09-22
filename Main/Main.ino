#define SERIAL_BAUD 115200

char serialBuffer[128];
char outputBuffer[8192];
char storageBuffer[8192]; 

unsigned long lastSensorReading = 0;
unsigned long lastBTTransmit = 0;
unsigned long lastStorageWrite = 0;
unsigned long lastBTReconnect = 0;
unsigned long lastHzReport = 0;


unsigned long loopCount = 0;
unsigned long lastHzTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    // Initialize buffers
    outputBuffer[0] = '\0';
    storageBuffer[0] = '\0';
    
    // Initialize Hz tracking
    lastHzTime = millis();
    
    pressureSetup();
    adxlSetup();
    storageSetup();
    connectionSetup();
    
    // Add CSV header with timestamp
    sprintf(serialBuffer, "pres,temp,x,y,z,timestamp\n");
    Serial.print("CSV Header: ");
    Serial.print(serialBuffer);
}

void loop() {
    loopCount++;
    
    if (checkTimer(lastSensorReading, 100)) {
        readSensorsToCSV();
    }
    
    if (checkTimer(lastBTTransmit, 10000)) {
        if (strlen(outputBuffer) > 0) {
            sprintf(serialBuffer, "Sending to BT (%d chars):\n", strlen(outputBuffer));
            Serial.print(serialBuffer);
            Serial.print(outputBuffer);
            
            if (btSendData(outputBuffer)) {
                // Successfully sent
            } else {
                sprintf(serialBuffer, "BT not connected, data cached\n");
                Serial.print(serialBuffer);
            }
                        
            // Clear the BT buffer
            outputBuffer[0] = '\0';
        }
    }

    if (!btIsConnected() && checkTimer(lastBTReconnect, 120000)) { // 2 minutes = 120000ms
        sprintf(serialBuffer, "Attempting BT reconnection...\n");
        Serial.print(serialBuffer);
        btReconnect();
    }

    if (checkTimer(lastStorageWrite, 3000)) {
        if (strlen(storageBuffer) > 0) {
            sprintf(serialBuffer, "Writing to storage (%d chars)\n", strlen(storageBuffer));
            Serial.print(serialBuffer);
            storageWrite(storageBuffer);
            storageBuffer[0] = '\0';
        }
    }

    if (checkTimer(lastHzReport, 5000)) {
        calculateAndReportHz();
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

void calculateAndReportHz() {
    unsigned long currentTime = millis();
    unsigned long timeDiff = currentTime - lastHzTime;
    
    if (timeDiff > 0) {
        float actualHz = (float)loopCount * 1000.0 / (float)timeDiff;
        sprintf(serialBuffer, "Runtime Hz: %.2f (loops: %lu, time: %lu ms)\n", 
                actualHz, loopCount, timeDiff);
        Serial.print(serialBuffer);
    }
    
    // Reset counters
    loopCount = 0;
    lastHzTime = currentTime;
}

void readSensorsToCSV() {
    char csvLine[256];
    float pressure, temperature;
    pressureRead(&pressure, &temperature);
    float x_g, y_g, z_g;
    adxlRead(&x_g, &y_g, &z_g);
    
    // Get current timestamp in milliseconds
    unsigned long timestamp = millis();
    
    sprintf(csvLine, "%.2f,%.2f,%.2f,%.2f,%.2f,%lu\n", 
            pressure, temperature, x_g, y_g, z_g, timestamp);
            
    if (strlen(outputBuffer) + strlen(csvLine) < sizeof(outputBuffer) - 1) {
        strcat(outputBuffer, csvLine);
    } else {
        sprintf(serialBuffer, "Output buffer full! Clearing...\n");
        Serial.print(serialBuffer);
        outputBuffer[0] = '\0';
        strcat(outputBuffer, csvLine);
    }
    if (strlen(storageBuffer) + strlen(csvLine) < sizeof(storageBuffer) - 1) {
        strcat(storageBuffer, csvLine);
    } else {
        sprintf(serialBuffer, "Storage buffer full! Clearing...\n");
        Serial.print(serialBuffer);
        storageBuffer[0] = '\0';
        strcat(storageBuffer, csvLine);
    }
    
    // Debug output
    sprintf(serialBuffer, "Sensor: %s", csvLine);
    Serial.print(serialBuffer);
}