// include libraries
#include "AS5600.h"

// include Modules
#include "context.h"
#include "i2c_interface.h"
#include "sensor_reader.h"
#include "mqtt_interface.h"
#include "health_monitor.h"
#include "actuator_manager.h"

#define ESP_01_SERIAL Serial1

AS5600 as5600;

// initialize context and modules
Context context;

SensorReader    sensorReader(as5600, context);
I2C_Interface   i2c_interface;
HealthMonitor   healthMonitor(context, sensorReader, i2c_interface);
MQTT_Interface  mqttInterface(ESP_01_SERIAL);
ActuatorManager actuatorManager(context, sensorReader, i2c_interface);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    ESP_01_SERIAL.begin(115200);

    // Initialize I2C communication
    i2c_interface.init();

    // recieve rotation target
    mqttInterface.subscribe("computer/out/health/info", HealthMonitor::sendHealthStatus);
    mqttInterface.subscribe("computer/out/rotation/target", ActuatorManager::onTargetRecieve);

    // perform health check
    healthMonitor.performHealthCheck();
}

void loop() {
    if (context.execute_movement){
        // listen for incoming messages
        bool targetReached = mqttInterface.loop();

        // send movment complete message if target is reached
        if (targetReached) {
            context.execute_movement = false;
            mqttInterface.publish("arduino/out/rotation/complete", "{}");
        }
    }
    
    // Add a small delay
    delay(100);
}