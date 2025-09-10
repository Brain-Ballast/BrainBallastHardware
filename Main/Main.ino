#define SERIAL_BAUD 115200

static char serialBuffer[128];

unsigned long lastPressure = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    pressureSetup();
    adxlSetup();

    // To be Tested //
    //batterySetup();
    //storageSetup();
}

void loop() {
    if (checkTimer(lastPressure, 1000)) { // every 1 second
        pressureStep();
        adxlStep();
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