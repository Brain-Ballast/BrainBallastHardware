#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;
extern char serialBuffer[128];

void pressureSetup() {
    Wire.begin();
    delay(50);
    
    sprintf(serialBuffer, "Starting MS5837...\n");
    Serial.print(serialBuffer);
    int attempts = 0;
    bool initialized = false;
    
    while (attempts < 5 && !initialized) {
        attempts++;
        
        sprintf(serialBuffer, "MS5837 init attempt %d...\n", attempts);
        Serial.print(serialBuffer);
        if (sensor.begin(0)) {
            initialized = true;
            sprintf(serialBuffer, "MS5837 initialized successfully!\n");
            Serial.print(serialBuffer);
            
            sprintf(serialBuffer, "Sensor type: MS5837-%02d\n", sensor.getType());
            Serial.print(serialBuffer);
        } else {
            sprintf(serialBuffer, "MS5837 init failed, retrying...\n");
            Serial.print(serialBuffer);
            Wire.end();
            delay(200);
            Wire.begin();
            delay(100);
        }
    }
    
    if (!initialized) {
        sprintf(serialBuffer, "MS5837 init failed after %d attempts! Check SDA/SCL wiring and power\n", attempts);
        Serial.print(serialBuffer);
        while(1) delay(50);
    }

    sprintf(serialBuffer, "MS5837 initialized with RobTillaart library\n");
    Serial.print(serialBuffer);
}

// Fast reading with adjustable OSR
// bits = 8  -> ~3ms  (OSR 256)  - lowest resolution, fastest
// bits = 9  -> ~5ms  (OSR 512)
// bits = 10 -> ~10ms (OSR 1024)
// bits = 11 -> ~20ms (OSR 2048)
// bits = 12 -> ~40ms (OSR 4096)
// bits = 13 -> ~80ms (OSR 8192) - highest resolution, slowest

void pressureRead(float* pressure, float* temperature) {
    // Use bits=9 for ~5ms read time (OSR 512) - good balance of speed/accuracy
    // For 20ms reads, use bits=8 for ~3ms
    int result = sensor.read(10);  // OSR 512 = ~5ms
    
    if (result == 0) {
        *pressure = sensor.getPressure();
        *temperature = sensor.getTemperature();
    } else {
        sprintf(serialBuffer, "MS5837 read error: %d\n", result);
        Serial.print(serialBuffer);
    }
}