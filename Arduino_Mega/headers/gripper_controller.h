#pragma once

#include <Servo.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"

class GripperController {
    public:

        GripperController(Context& context, MQTT_Interface& mqttInterface);

        void        init();
        static void setGripperState(const String& topic, const JsonDocument& payload);
        static void getGripperState(const String& topic, const JsonDocument& payload);

    private:

        static GripperController* instance_;

        Context& context_;
        MQTT_Interface& mqttInterface_;

        Servo leftFinger_;
        Servo rightFinger_;

};