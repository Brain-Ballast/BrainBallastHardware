#include <SparkFun_ADXL345.h>

ADXL345 adxl = ADXL345();

extern char serialBuffer[128];

void adxlSetup() {
    sprintf(serialBuffer, "Starting ADXL345...\n");
    Serial.print(serialBuffer);

    accel.powerOn();

    // Set rosolution [2g, 4g, 8g, 16g]
    accel.setRangeSetting(16);

    // Set rate (0.10Hz to 3200Hz)
    adxl.setRate(100); // 100Hz

    Serial.print("ADXL345 initialized\n");
}

void adxlStep() {
    int x, y, z;
    accel.readAccel(&x, &y, &z);

    // Sensitivity (g per LSB) = Full Scale Range / 2^(Resolution) == [3.9, 7.8, 15.6, 31.2]
    float x_g = x * 0.0039f;
    float y_g = y * 0.0039f;
    float z_g = z * 0.0039f;

    sprintf(serialBuffer, "Accel X: %.2f g | Y: %.2f g | Z: %.2f g\n", x_g, y_g, z_g);
    Serial.print(serialBuffer);
}

// Optomiziations //
// Set activity/inactivity thresholds (0-255)
// adxl.setActivityThreshold(75); // 62.5mg per increment
// adxl.setInactivityThreshold(75); // 62.5mg per increment
// adxl.setTimeInactivity(10); // How many seconds of no activity is inactive?

// // Look for taps on axes
// adxl.setTapThreshold(50); // 62.5mg per increment
// adxl.setTapDuration(15); // 625μs per increment

// // Turn on interrupts for features (activities, inactivity, tap, freefall)
// adxl.setActivityX(1);
// adxl.setActivityY(1);
// adxl.setActivityZ(1);
