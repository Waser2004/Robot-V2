// include libraries
#include "AS5600.h"
#include "Servo.h"

// include Modules
#include "context.h"
#include "i2c_interface.h"
#include "sensor_reader.h"
#include "mqtt_interface.h"
#include "health_monitor.h"
#include "actuator_manager.h"
#include "gripper_controller.h"

#define ESP_01_SERIAL Serial1

AS5600 as5600;
Servo leftFinger;
Servo rightFinger;

// initialize context and modules
Context           context;

I2C_Interface     i2c_interface;
MQTT_Interface    mqttInterface(ESP_01_SERIAL, context); 

SensorReader      sensorReader(as5600, context);
HealthMonitor     healthMonitor(context, sensorReader, i2c_interface, mqttInterface);
ActuatorManager   actuatorManager(context, sensorReader, i2c_interface, mqttInterface);
GripperController gripperController(context, mqttInterface, leftFinger, rightFinger);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    ESP_01_SERIAL.begin(115200);

    // attach servos
    leftFinger.attach(2);
    rightFinger.attach(3);

    // initialize modules
    sensorReader.init();
    i2c_interface.init();

    // recieve messages
    mqttInterface.subscribe("computer/out/checkup", MQTT_Interface::onCheckupReceive);
    mqttInterface.subscribe("computer/out/health/info", HealthMonitor::sendHealthStatus);
    
    mqttInterface.subscribe("computer/out/rotation/path", ActuatorManager::onPathRecieve);
    mqttInterface.subscribe("computer/out/rotation/info", ActuatorManager::onActuatorInfo);
    mqttInterface.subscribe("computer/out/rotation/clear", ActuatorManager::onRotationClear);
    mqttInterface.subscribe("computer/out/rotation/target", ActuatorManager::onTargetRecieve);

    mqttInterface.subscribe("computer/out/gripper/info", GripperController::getGripperState); 
    mqttInterface.subscribe("computer/out/gripper/target", GripperController::setGripperState);

    delay(1000); // wait for everything to stabilize

    // perform health check
    healthMonitor.performHealthCheck();
}

void loop() {
    // loop rotation
    if (context.execute_movement && context.good_health_check) {
        actuatorManager.loop();
    }

    // loop modules
    mqttInterface.loop();      // listen for incoming messages
    gripperController.loop();  // write gripper state to servos
}
