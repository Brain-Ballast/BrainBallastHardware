// SENDING DATA OVER ASCII

#define SERIAL_BAUD 115200

// Global variable definitions
char serialBuffer[128];
char outputBuffer[32768];  // Increased buffer size
char storageBuffer[65536]; // Increased buffer size
char lastCSVLine[128];     // Store last sensor reading for printing

unsigned long lastSensorReading = 0;
unsigned long lastBTTransmit = 0;
unsigned long lastLoRaTransmit = 0;
unsigned long lastStorageWrite = 0;
unsigned long lastBTReconnect = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastSerialPrint = 0;
unsigned long startTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    startTime = millis();
    
    // Initialize buffers
    outputBuffer[0] = '\0';
    storageBuffer[0] = '\0';
    lastCSVLine[0] = '\0';
    
    pressureSetup();
    adxlSetup();
    storageSetup();
    connectionSetup();
    loRaSetup();
    
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
    // Send LoRa data every 10 seconds
    if (checkTimer(lastLoRaTransmit, 10000)) {
        if (strlen(outputBuffer) > 0) {
            if (loRaSendData(outputBuffer)) {
                outputBuffer[0] = '\0';  // Clear buffer after send
                Serial.print("Sending LoRa: \n");
            } else {
                Serial.printf("LoRa busy, data cached (%d chars)\n", strlen(outputBuffer));
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
    
    if (checkTimer(lastBTReconnect, 40000)) {
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

/*////////////////////////////////////////////////////////////////////////////////////////////////
     USING SEND DATA OVER BYTES OVER LORA, ALSO COMMENTS OUT SENDING AND CONNECTING OVER BLUETOOTH
                 COMMENT OUT THE ABOVE OR BELOW FOR WHAT WAY TO SEND DATA OVER LORA
*/////////////////////////////////////////////////////////////////////////////////////////////////

#define SERIAL_BAUD 115200

// Global variable definitions
char serialBuffer[128];
char outputBuffer[32768];  // Increased buffer size
uint8_t LoRaBuffer[160];  // Increased buffer size
char storageBuffer[65536]; // Increased buffer size
char lastCSVLine[128];     // Store last sensor reading for printing
uint8_t LoRaBufferBinary[65536];
uint16_t binaryBufferIndex = 0;
uint16_t LoRaBufferIndex = 0;


unsigned long lastSensorReading = 0;
unsigned long lastBTTransmit = 0;
unsigned long lastLoRaTransmit = 0;
unsigned long lastStorageWrite = 0;
unsigned long lastBTReconnect = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastSerialPrint = 0;
unsigned long startTime = 0;

typedef struct {
    float pressure;
    float temperature;
    float x_g;
    float y_g;
    float z_g;
    uint32_t timestamp;
} SensorData;

SensorData sensorRead;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    startTime = millis();
    
    // Initialize buffers
    LoRaBuffer[0] = '\0';
    outputBuffer[0] = '\0';
    storageBuffer[0] = '\0';
    lastCSVLine[0] = '\0';
    
    pressureSetup();
    //Serial.print("Pressure Setup\n");
    //adxlSetup();
    //Serial.print("ADXL Setup\n");
    storageSetup();
    //Serial.print("Storage Setup\n");
    //connectionSetup();
    //Serial.print("Connection Setup\n");
    loRaSetup(); // Initialize the lora module
    //Serial.print("LoRa Setup\n");
    
    // Add CSV header
    sprintf(serialBuffer, "pres,temp,x,y,z,timestamp\n");
    Serial.print("CSV Header: ");
    //Serial.print(serialBuffer);
}

void loop() {
    // Read BOTH sensors every 20ms (50Hz)
    lastSensorReading = millis();

    if (lastSensorReading, 20) {
        //readSensorsToCSV();
        readSensorsToBinary();
    }
    
    // Check commands less frequently
    // if (checkTimer(lastCommandCheck, 100)) {
    //     btHandleCommands();
    // }
    
    // Send to BT every 10 seconds
    // if (checkTimer(lastBTTransmit, 10000)) {
    //     if (strlen(outputBuffer) > 0) {
    //         if (btSendData(outputBuffer) && loRaSendData(outputBuffer)) {
    //             // Successfully sent, clear buffer
    //             outputBuffer[0] = '\0';
    //         } else {
    //             // BT not connected, keep data cached
    //             sprintf(serialBuffer, "BT not connected, data cached (%d chars)\n", strlen(outputBuffer));
    //             Serial.print(serialBuffer);
    //         }
    //     }
    // }

    // if (checkTimer(lastLoRaTransmit, 100)) {
    //     if (strlen(LoRaBuffer) > 0) {
    //         if (loRaSendData(LoRaBuffer)) {
    //             //Serial.print(storageBuffer);
    //             LoRaBuffer[0] = '\0';  // Clear buffer after send
    //             //Serial.print("Sending LoRa: \n");
    //         } else {
    //             Serial.printf("LoRa busy, data cached (%d chars)\n", strlen(LoRaBuffer));
    //         }
    //     }
    // }

    // Send to LoRa every 100ms
// Send to LoRa every 100ms
    if (checkTimer(lastLoRaTransmit, 100)) {
        if (LoRaBufferIndex > 0) {
            if (loRaSendData((const uint8_t*)LoRaBuffer, LoRaBufferIndex)) {
                LoRaBufferIndex = 0;  // Clear buffer after send
                //Serial.println("Adding to LoRa \n");
            } else {
                Serial.printf("LoRa busy, data cached (%d bytes)\n", LoRaBufferIndex);
            }
        }
    }

    // Write to SD card every 30 seconds
    // if (checkTimer(lastStorageWrite, 30000)) {
    //     if (strlen(storageBuffer) > 0) {
    //         sprintf(serialBuffer, "Writing to storage (%d chars)\n", strlen(storageBuffer));
    //         //Serial.print(serialBuffer);
    //         storageWrite(storageBuffer);
    //         storageBuffer[0] = '\0';
    //     }
    // }
    
    // if (checkTimer(lastBTReconnect, 40000)) {
    //     if (!btIsConnected()) {
    //         btReconnect();
    //     }
    // }
    
    // Print status every 500ms
    // if (checkTimer(lastSerialPrint, 500)) {
    //     printStatus();
    // }
}

bool checkTimer(unsigned long &lastTrigger, unsigned long interval) {
    unsigned long now = millis();
    if (now - lastTrigger >= interval) {
        lastTrigger = now;
        return true;
    }
    return false;
}

void readSensorsToBinary() {
    static int sensorCount = 0;

    pressureRead(&sensorRead.pressure, &sensorRead.temperature);
    adxlRead(&sensorRead.x_g, &sensorRead.y_g, &sensorRead.z_g);
    sensorRead.timestamp = millis();

    // Add to binary buffer
    if (binaryBufferIndex + sizeof(SensorData) < sizeof(binaryBufferIndex)) {
        memcpy(&LoRaBufferBinary[binaryBufferIndex], &sensorRead, sizeof(SensorData));
        binaryBufferIndex += sizeof(SensorData);
    } else {
        binaryBufferIndex = 0;
        memcpy(&LoRaBufferBinary[binaryBufferIndex], &sensorRead, sizeof(SensorData));
        binaryBufferIndex += sizeof(SensorData);
    }
    
    // Print every 25th reading (25 * 20ms = 500ms)
    // sensorCount++;
    // if (sensorCount % 25 == 0) {
    //     sprintf(lastCSVLine, "%.2f,%.2f,%.2f,%.2f,%.2f,%lu\n", 
    //             sensorRead.pressure, sensorRead.temperature, 
    //             sensorRead.x_g, sensorRead.y_g, sensorRead.z_g, 
    //             sensorRead.timestamp);
    //     Serial.print(lastCSVLine);
    // }

    
//     // ALWAYS add to LoRa buffer
//     if (LoRaBufferIndex + sizeof(SensorData) < sizeof(LoRaBuffer)) {
//         memcpy(&LoRaBuffer[LoRaBufferIndex], &sensorRead, sizeof(SensorData));
//         LoRaBufferIndex += sizeof(SensorData);
//         //Serial.println("Adding to lora buffer");
//     } else {
//         LoRaBufferIndex = 0;  // Reset buffer
//         memcpy(&LoRaBuffer[LoRaBufferIndex], &sensorRead, sizeof(SensorData));
//         LoRaBufferIndex += sizeof(SensorData);
//     }
// }

// void readSensorsToCSV() {
//     static int sensorCount = 0;
    
//     // Read both sensors
//     float pressure, temperature;
//     pressureRead(&pressure, &temperature);
    
//     float x_g, y_g, z_g;
//     adxlRead(&x_g, &y_g, &z_g);
    
//     unsigned long timestamp = millis();
    
//     // Create CSV line and store it globally for printing
//     sprintf(lastCSVLine, "%.2f,%.2f,%.2f,%.2f,%.2f,%lu\n", 
//             pressure, temperature, x_g, y_g, z_g, timestamp);
//     //sprintf(lastCSVLine, "%.2f,%lu\n", pressure, timestamp);
//     // Print every 25th reading (25 * 20ms = 500ms)
//     sensorCount++;
//     if (sensorCount % 25 == 0) {
//         Serial.print(lastCSVLine);
//     }
    
//     // Only add to BT buffer if connected

//     // if (btIsConnected()) {
//     //     if (strlen(outputBuffer) + strlen(lastCSVLine) < sizeof(outputBuffer) - 1) {
//     //         strcat(outputBuffer, lastCSVLine);
//     //     } else {
//     //         sprintf(serialBuffer, "WARNING: BT buffer full, clearing old data\n");
//     //         Serial.print(serialBuffer);
//     //         outputBuffer[0] = '\0';
//     //         strcat(outputBuffer, lastCSVLine);
//     //     }
//     // }

//     // ALWAYS add to LoRa
//     if (strlen(LoRaBuffer) + strlen(lastCSVLine) < sizeof(LoRaBuffer) - 1) {
//         strcat(LoRaBuffer, lastCSVLine);
//         //Serial.print(LoRaBuffer);
//     } else {
//         sprintf(LoRaBuffer, "WARNING: Storage buffer full, clearing old data\n");
//         Serial.print(LoRaBuffer);
//         storageBuffer[0] = '\0';
//         strcat(LoRaBuffer, lastCSVLine);
//     }
    
//     // ALWAYS add to storage buffer (regardless of BT connection)
//     // if (strlen(storageBuffer) + strlen(lastCSVLine) < sizeof(storageBuffer) - 1) {
//     //     strcat(storageBuffer, lastCSVLine);
//     // } else {
//     //     sprintf(serialBuffer, "WARNING: Storage buffer full, clearing old data\n");
//     //     Serial.print(serialBuffer);
//     //     storageBuffer[0] = '\0';
//     //     strcat(storageBuffer, lastCSVLine);
//     // }
// }

void printStatus() {
    static int printCount = 0;
    printCount++;
    
    // Print buffer status every 500ms
    sprintf(serialBuffer, "[Status] BT=%d chars, Storage=%d chars, Connected=%s\n", 
            (int)strlen(outputBuffer), (int)strlen(storageBuffer), 
            btIsConnected() ? "YES" : "NO");
    Serial.print(serialBuffer);
}
