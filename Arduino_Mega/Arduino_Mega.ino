// include libraries
#include "AS5600.h"

// include Modules
#include "context.h"
#include "i2c_interface.h"
#include "sensor_reader.h"
#include "mqtt_interface.h"
#include "health_monitor.h"

#define ESP_01_SERIAL Serial1

AS5600 as5600;

// initialize context and modules
Context context;

SensorReader   sensorReader(as5600, context);
I2C_Interface  i2c_interface;
HealthMonitor  healthMonitor(context, sensorReader, i2c_interface);
MQTT_Interface mqttInterface(ESP_01_SERIAL);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    ESP_01_SERIAL.begin(115200);

    // Initialize I2C communication
    i2c_interface.init();

    // perform health check
    healthMonitor.performHealthCheck();
}

void loop() {
    // listen for incoming messages
    mqttInterface.loop();

    // Add a small delay to avoid flooding the serial output
    delay(100);
}