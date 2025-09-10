#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;

void pressureSetup() {
    Wire.begin();

    sprintf(serialBuffer, "Starting MS5837...\n");
    Serial.print(serialBuffer);

    while (!sensor.init()) {
        sprintf(serialBuffer, "MS5837 init failed! Check SDA/SCL wiring\n");
        Serial.print(serialBuffer);
        delay(2000);
    }

    sensor.setModel(MS5837::MS5837_30BA);
    sensor.setFluidDensity(997); // 997 = freshwater, 1029 = seawater

    sprintf(serialBuffer, "MS5837 initialized\n");
    Serial.print(serialBuffer);
}

void pressureStep() {
    sensor.read();

    sprintf(serialBuffer,
            "Pressure: %.2f mbar | Temp: %.2f C | Depth: %.2f m | Altitude: %.2f m\n",
            sensor.pressure(),
            sensor.temperature(),
            sensor.depth(),
            sensor.altitude());

    Serial.print(serialBuffer);
}

// Optomizations //
// OSR = Over Sampling Ratio
// Higher OSR = more accurate, but slower and more power
// Lower OSR = faster, less power, but less accurate
// Valid values: MS5837_OSR_256, MS5837_OSR_512, MS5837_OSR_1024, 
//               MS5837_OSR_2048, MS5837_OSR_4096, MS5837_OSR_8192
//sensor.setPressureOSR(osr);
