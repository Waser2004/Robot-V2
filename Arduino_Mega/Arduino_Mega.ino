#include "mqtt_interface.h"

#define ESP_01_SERIAL Serial1

MQTT_Interface mqttInterface(ESP_01_SERIAL);

// test callback function
void testCallback(const String& topic, const String& payload) {
    Serial.print("Received on topic: ");
    Serial.print(topic);
    Serial.print(" with payload: ");
    Serial.println(payload);
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    ESP_01_SERIAL.begin(115200);

    mqttInterface.subscribe("test", testCallback);
    mqttInterface.publish("test", "Hello, MQTT!");
}

void loop() {
    // Call the loop method to handle incoming messages
    mqttInterface.loop();

    // Add a small delay to avoid flooding the serial output
    delay(100);
}