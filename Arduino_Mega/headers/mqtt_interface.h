/*
    The mqtt interface handles all the communication between the Arduino Mega and the ESP-01.

    The message format is as follows:
    - Publish: "pub <topic> <payload>"
    - Subscribe: "sub <topic> <payload>"
*/
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

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
        explicit MQTT_Interface(Stream& transport, size_t maxSubs = 10);

        // loop method
        void loop();

        // pub & sub methods
        bool publish(const String& topic, const String& payload);
        bool subscribe(const String& filter, const MQTT_Callback& callback);

    private:
        // transport stream
        Stream& _serial;

        // subscriptions
        Sub* _subs = nullptr;
        size_t _maxSubs;
        size_t _numSubs = 0;

        // input buffer
        String _inputBuffer;

        // handle incoming messages
        void handleMessage(const String& message);
};