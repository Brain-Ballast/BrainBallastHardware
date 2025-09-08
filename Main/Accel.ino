#include <Adafruit_ADXL345_U.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

void adxlSetup() {
    sprintf(serialBuffer, "Starting ADXL345...\n");
    Serial.print(serialBuffer);

    if (!accel.begin()) {
        sprintf(serialBuffer, "ADXL345 not detected. Check wiring!\n");
        Serial.print(serialBuffer);
        while (1);
    }

    accel.setRange(ADXL345_RANGE_16_G);  // Example range
    sprintf(serialBuffer, "ADXL345 initialized\n");
    Serial.print(serialBuffer);
}

void adxlStep() {
    sensors_event_t event;
    accel.getEvent(&event);

    sprintf(serialBuffer,
            "Accel X: %.2f | Y: %.2f | Z: %.2f m/s^2\n",
            event.acceleration.x,
            event.acceleration.y,
            event.acceleration.z);
    Serial.print(serialBuffer);
}
