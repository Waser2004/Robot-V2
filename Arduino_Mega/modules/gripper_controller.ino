#include <Servo.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"
#include "gripper_controller.h"

#define LEFT_FINGER_PIN 2
#define RIGHT_FINGER_PIN 3

GripperController::GripperController(Context& context, MQTT_Interface& mqttInterface)
    : context_(context), mqttInterface_(mqttInterface) {
    instance_ = *this;
}

GripperController::init(){
    // Initialize the servos for the gripper
    leftFinger_.attach(LEFT_FINGER_PIN);
    rightFinger_.attach(RIGHT_FINGER_PIN);

    // open gripper by default
    setgripperState(true);
}

GripperController::setGripperState(const String& topic, const JsonDocument& payload) {
    /*
        This function open and closes the gripper based on the input parameter.
    */
    // check for instance
    if (!instance_) return;

    // update gripper state
    bool open = payload["open"].as<bool>();
    context_.current_gripper_state = open;

    if (open) {
        // Open the gripper
        leftFinger_.write(0);
        rightFinger_.write(90);
    } else {
        // Close the gripper
        leftFinger_.write(90);
        rightFinger_.write(0);
    }

    // publish gripper complte
    mqttInterface_.publish("arduino/out/gripper/complete", "{}");
}

GripperController::getGripperState(const String& topic, const JsonDocument& payload) {
    /*
        This function is called when a gripper state request message is received.
        It returns the current state of the gripper.
    */
    // check for instance
    if (!instance_) return;
    
    // Publish the current gripper state
    String payload = "{\"open\": " + String(instance_.context_.current_gripper_state ? "true" : "false") + "}";
    mqttInterface_.publish("arduino/out/gripper/state", payload);
}