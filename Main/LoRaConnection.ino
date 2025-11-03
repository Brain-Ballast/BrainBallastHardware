#include "SPI.h"
#include "LoRa.h"

// Set up the LoRa Pins
#define LORA_SS    3
#define LORA_RST   2
#define LORA_DIO0  4
#define LORA_SCK   8
#define LORA_MISO  9
#define LORA_MOSI  10

extern char serialBuffer[128];

bool loraInitialized = false;
bool loraBusy = false;

void loRaSetup() {
  Serial.print("LoRa is Setup\n");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS); // initialize the SPI
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0); // set the lora pins

  // Try to establish the LoRa
  if (!LoRa.begin(915E6)) {
    Serial.println("LoRa init failed!");
    while (true);
  }
  Serial.println("LoRa initialized.");
  loraInitialized = true;
}

// Send the buffer data when called
bool loRaSendData(const char* data) {

  // check if LoRa initialized
  if (!loraInitialized || loraBusy) {
    Serial.print("LoRa Not Initialized or Busy\n");
    return false;

  } 

  // if there is nothing in the data then send nothing
  int len = strlen(data);
  if (len == 0) return false;

  loraBusy = true;
  const int maxChunk = 200;
  // TODO: Fix this to where it sends in bits not in ASCII
  for (int i = 0; i < len; i += maxChunk) {
    int chunkLen = min(maxChunk, len - i);
    LoRa.beginPacket();
    LoRa.write((const uint8_t*)(data + i), chunkLen); // send the packet
    LoRa.endPacket();
    delay(10);
  }
  loraBusy = false;
  Serial.printf("LoRa sent %d bytes\n", len); // output when the packet is sent to serial
  return true;
}
