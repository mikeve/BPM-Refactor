#include "WiFiManager.h"

const int SD_CHIP_SELECT = 10;

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Initialize the SD card
    SD.begin(SD_CHIP_SELECT);

    InitWiFiManager();
}

void loop() {
    ServiceWiFiManager();
}
