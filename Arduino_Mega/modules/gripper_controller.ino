#include <Servo.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"
#include "gripper_controller.h"

#define LEFT_FINGER_PIN 2
#define RIGHT_FINGER_PIN 3

GripperController* GripperController::instance_ = nullptr;

GripperController::GripperController(Context& context, MQTT_Interface& mqttInterface, Servo& leftFinger, Servo& rightFinger)
    : context_(context), mqttInterface_(mqttInterface), leftFinger_(leftFinger), rightFinger_(rightFinger) {
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

void GripperController::loop() {
    /*
        This method is called in the main loop to actively write the gripper postion to the servos.
    */
    if (context_.current_gripper_state) {
        // Open the gripper
        leftFinger_.write(15);
        rightFinger_.write(85);
    } else {
        // Close the gripper
        leftFinger_.write(100);
        rightFinger_.write(0);
    }
}

void GripperController::setGripperState(const String& topic, const JsonDocument& payload) {
    /*
        This function open and closes the gripper based on the input parameter.
    */
    // check for instance
    if (!instance_) return;

    // publish gripper target received
    instance_->mqttInterface_.publish("arduino/confirm/gripper/target", "{}");

    // update gripper state
    bool open = payload["open"].as<bool>();
    instance_->context_.current_gripper_state = open;

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