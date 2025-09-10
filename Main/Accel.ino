#include <SparkFun_ADXL345.h>

ADXL345 accel;

extern char serialBuffer[128];

void adxlSetup() {
    sprintf(serialBuffer, "Starting ADXL345...\n");
    Serial.print(serialBuffer);

    accel.powerOn();

    // Set resolution 2, 4, 8, 16g
    accel.setRangeSetting(16);

    sprintf(serialBuffer, "ADXL345 initialized\n");
    Serial.print(serialBuffer);
}

void adxlStep() {
    int x, y, z;
    accel.readAccel(&x, &y, &z);

    // Convert raw LSB to g (~3.9 mg/LSB in full res mode)
    float x_g = x * 0.0039f;
    float y_g = y * 0.0039f;
    float z_g = z * 0.0039f;

    sprintf(serialBuffer, "Accel X: %.2f g | Y: %.2f g | Z: %.2f g\n", x_g, y_g, z_g);
    Serial.print(serialBuffer);
}
