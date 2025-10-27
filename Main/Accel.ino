#include <SparkFun_ADXL345.h>

ADXL345 adxl = ADXL345();
extern char serialBuffer[128];

void adxlSetup() {
    sprintf(serialBuffer, "Starting ADXL345...\n");
    Serial.print(serialBuffer);
    adxl.powerOn();
    // Set range [2g, 4g, 8g, 16g]
    adxl.setRangeSetting(16);
    // Set rate to 100Hz
    adxl.setRate(100); 

    sprintf(serialBuffer, "ADXL345 initialized\n");
    Serial.print(serialBuffer);
}

// Updated function to just read and return values
void adxlRead(float* x_g, float* y_g, float* z_g) {
    int x, y, z;
    adxl.readAccel(&x, &y, &z);

    // Calculate sensitivity based on 16g range                       [2g , 4g , 8g  , 16g]
    // Sensitivity (g per LSB) = Full Scale Range / 2^(Resolution) == [3.9, 7.8, 15.6, 31.2]
    float sensitivity = 31.2f / 1000.0f; // Convert mg to g
    
    *x_g = x * sensitivity;
    *y_g = y * sensitivity;
    *z_g = z * sensitivity;
}