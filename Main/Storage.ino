#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Add this line to resolve the 'fs::FS' errors
using namespace fs;

void listDir(FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

// ... the rest of your functions (createDir, removeDir, etc.) will now work correctly
// as long as you change `fs::FS` to just `FS` in their declarations, or use the
// `using namespace fs;` method above.