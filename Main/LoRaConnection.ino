// USING LORA WITH ASCII

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
// // USING LORA SENDING BYTES
// #include "SPI.h"
// #include "LoRa.h"

// // LoRa Pin Definitions (adjust as per your wiring)
// #define LORA_SS    3
// #define LORA_RST   2
// #define LORA_DIO0  4
// #define LORA_SCK   8
// #define LORA_MISO  9
// #define LORA_MOSI  10

// extern char serialBuffer[128];

// bool loraInitialized = false;
// bool loraBusy = false;

// void loRaSetup() {
//   Serial.print("LoRa is Setup\n");
//   SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
//   LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
//   if (!LoRa.begin(915E6)) {
//     Serial.println("LoRa init failed!");
//     while (true);
//   }
//   Serial.println("LoRa initialized.");
//   loraInitialized = true;
// }

// // bool loRaSendData(const char* data) {
// //   if (!loraInitialized || loraBusy) {
// //     Serial.print("LoRa Not Initialized or Busy\n");
// //     return false;

// //   } 
  
// //   int len = strlen(data);
// //   if (len == 0) return false;

// //   loraBusy = true;
// //   const char* prefix = "    ";
// //   const int maxChunk = 200;

// //   char sendBuffer[maxChunk + 5];

// //   for (int i = 0; i < len; i += maxChunk) {
// //     int chunkLen = min(maxChunk, len - i);
// //     // Write what the buffer is when it is interrupted

// //     // Build chunk with four spaces prepended
// //     memcpy(sendBuffer, prefix, 4); // Copy four spaces
// //     memcpy(sendBuffer + 4, data + i, chunkLen); // Copy chunk data
// //     sendBuffer[chunkLen + 4] = '\0'; // Null terminate (not strictly needed for LoRa.write but safe)
// //     LoRa.beginPacket();
// //     LoRa.write((const uint8_t*)sendBuffer, chunkLen + 4);
// //     LoRa.endPacket();
// //     //delayMicroseconds(50);
// //     // LoRa.beginPacket();
// //     // LoRa.write((const uint8_t*)(data + i), chunkLen);
// //     // LoRa.endPacket();
// //     //delay(5);
// //   }
// //   loraBusy = false;
// //   //Serial.printf("LoRa sent %d bytes\n", len);
// //   return true;
// // }

// bool loRaSendData(const uint8_t* data, uint16_t length) {
//   if (!loraInitialized || loraBusy) {
//     Serial.print("LoRa Not Initialized or Busy\n");
//     return false;
//   } 
  
//   if (length == 0) return false;

//   loraBusy = true;
//   const char* prefix = "    ";
//   const int maxChunk = 200;
//   uint8_t sendBuffer[maxChunk + 4];

//   for (int i = 0; i < length; i += maxChunk) {
//     int chunkLen = min(maxChunk, (int)(length - i));
    
//     // Copy four spaces prefix
//     memcpy(sendBuffer, prefix, 4);
//     // Copy chunk data
//     memcpy(sendBuffer + 4, data + i, chunkLen);
    
//     LoRa.beginPacket();
//     Serial.print(sendBuffer);
//     Serial.print("\n");
//     LoRa.write(sendBuffer, chunkLen + 4);
//     //Serial.print("Lora Sent");
//     LoRa.endPacket();
//   }
//   loraBusy = false;
//   return true;
// }
