// Battery measurement from battery VIN to A0 with 100kohm resistor

#define BATTERY_PIN A0   // Same as D0


int batteryPercent(float v) {
  if (v >= 3.0) return 100;
  if (v <= 2.0) return 0;
  return (int)((v - 2.0) / (3.0 - 2.0) * 100);
}

// void batterysetup() {
//   Serial.println("Battery monitor started...");
// }

void batteryStep() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  int percent = batteryPercent(voltage);
  Serial.printf("Battery: %.2f V (%d%%)\n", voltage, percent);
}
