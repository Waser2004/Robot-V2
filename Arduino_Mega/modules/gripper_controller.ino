#include <Servo.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"
#include "gripper_controller.h"

#define LEFT_FINGER_PIN 2
#define RIGHT_FINGER_PIN 3

GripperController* GripperController::instance_ = nullptr;

GripperController::GripperController(Context& context, MQTT_Interface& mqttInterface)
    : context_(context), mqttInterface_(mqttInterface) {
    instance_ = this;
}

void GripperController::init(){
    // Initialize the servos for the gripper
    leftFinger_.attach(LEFT_FINGER_PIN);
    rightFinger_.attach(RIGHT_FINGER_PIN);

    // create payload for gripper state to be open
    JsonDocument payload;
    payload["open"] = true;

    // open gripper by default
    setGripperState("", payload);
}

void GripperController::setGripperState(const String& topic, const JsonDocument& payload) {
    /*
        This function open and closes the gripper based on the input parameter.
    */
    // check for instance
    if (!instance_) return;

    // update gripper state
    bool open = payload["open"].as<bool>();
    instance_->context_.current_gripper_state = open;

    if (open) {
        // Open the gripper
        instance_->leftFinger_.write(0);
        instance_->rightFinger_.write(90);
    } else {
        // Close the gripper
        instance_->leftFinger_.write(90);
        instance_->rightFinger_.write(0);
    }

    // publish gripper complte
    instance_->mqttInterface_.publish("arduino/out/gripper/complete", "{}");
}

void GripperController::getGripperState(const String& topic, const JsonDocument& payload) {
    /*
        This function is called when a gripper state request message is received.
        It returns the current state of the gripper.
    */
    // check for instance
    if (!instance_) return;
    
    // Publish the current gripper state
    String sendPayload = "{\"open\": " + String(instance_->context_.current_gripper_state ? "true" : "false") + "}";
    instance_->mqttInterface_.publish("arduino/out/gripper/state", sendPayload);
}