#include "context.h"
#include "mqtt_interface.h"

#define ESP_01_SERIAL Serial1

Context context;
MQTT_Interface mqttInterface(ESP_01_SERIAL);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    ESP_01_SERIAL.begin(115200);
}

void loop() {
    // listen for incoming messages
    mqttInterface.loop();

    // Add a small delay to avoid flooding the serial output
    delay(100);
}