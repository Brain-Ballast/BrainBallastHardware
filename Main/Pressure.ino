#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;
extern char serialBuffer[128];

void pressureSetup() {
    Wire.begin();

    sprintf(serialBuffer, "Starting MS5837...\n");
    Serial.print(serialBuffer);

    while (!sensor.init()) {
        sprintf(serialBuffer, "MS5837 init failed! Check SDA/SCL wiring\n");
        Serial.print(serialBuffer);
        delay(50);
    }

    sensor.setModel(MS5837::MS5837_30BA);
    sensor.setFluidDensity(997); // 997 = freshwater, 1029 = seawater
    sprintf(serialBuffer, "MS5837 initialized\n");
    Serial.print(serialBuffer);
}

// Updated function to just read and return values
void pressureRead(float* pressure, float* temperature) {
    sensor.read();
    *pressure = sensor.pressure();
    *temperature = sensor.temperature();
}