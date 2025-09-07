/*
    The mqtt interface handles all the communication between the Arduino Mega and the ESP-01.

    The message format is as follows:
    - Publish: "pub <topic> <payload>"
    - Subscribe: "sub <topic> <payload>"
*/
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"

// types
typedef void (*MQTT_Callback)(const String& topic, const JsonDocument& payload);
typedef struct Sub {
    String filter;
    MQTT_Callback callback;
};

// MQTT interface class
class MQTT_Interface {
    public:
        // constructor
        explicit    MQTT_Interface(Stream& transport, Context& context, size_t maxSubs = 10);

        // loop method
        void        loop();
        void        sendCheckup();
        static void onCheckupReceive(const String& topic, const JsonDocument& payload);


        // pub & sub methods
        bool        publish(const String& topic, const String& payload);
        bool        subscribe(const String& filter, const MQTT_Callback& callback);

    private:

        static MQTT_Interface* instance_;

        // transport stream
        Stream& serial_;
        Context& context_;

        // subscriptions
        Sub*   subs_ = nullptr;
        size_t maxSubs_;
        size_t numSubs_ = 0;

        // input buffer
        char   inputBuffer_[512];
        size_t inputBufferHead_ = 0;

        // software flow control state
        bool   xoffSent_ = false;
        void   handleFlowControl();

        // handle incoming messages (accept C-string to avoid String allocations)
        void handleMessage();
};
