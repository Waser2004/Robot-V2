#include <Arduino.h>
#include <ArduinoJson.h>

#include "mqtt_interface.h"

// Constructor implementation
MQTT_Interface::MQTT_Interface(Stream& transport, size_t maxSubs) 
    : _serial(transport), _maxSubs(maxSubs)
{
    _subs = new Sub[_maxSubs];
}

// Publish method implementation
bool MQTT_Interface::publish(const String& topic, const String& payload) {
    // pub payload
    _serial.print(F("pub "));
    _serial.print(topic);
    _serial.print(" ");
    _serial.println(payload);

    // pub complete return true
    return true;
}

// Subscribe method implementation
bool MQTT_Interface::subscribe(const String& filter, const MQTT_Callback& callback) {
    // check if subscriptions are full
    if (_numSubs >= _maxSubs) {
        return false;
    }

    // add subscription
    _subs[_numSubs].filter = filter;
    _subs[_numSubs].callback = callback;
    _numSubs++;

    // subscription complete return true
    return true;
}

void MQTT_Interface::loop() {
    while (_serial.available()) {
        //read char
        char c = (char)_serial.read();

        // end of line
        if (c == '\n') {
            // move input from buffer to message variable
            String message = _inputBuffer;
            _inputBuffer = "";

            // handle message
            message.trim();
            if (message.length()) {
                handleMessage(message);
            }
        }
        // add char to input buffer
        else {
            _inputBuffer += c;
        }
    }
}

void MQTT_Interface::handleMessage(const String& message) {
    // check if message starts with "sub" - skip subscription messages
    if (message.startsWith(F("sub "))) {
        return;
    }

    // get space positions
    int firstSpace = message.indexOf(' ');
    int secondSpace = message.indexOf(' ', firstSpace + 1);

    // check if message is valid format
    if (firstSpace < 0 || secondSpace < 0) {
        return;
    }

    // extract topic and payload
    String topic = message.substring(firstSpace + 1, secondSpace);
    String payload = message.substring(secondSpace + 1);

    // deserialize JSON payload
    JsonDocument jsonPayload;
    DeserializationError error = deserializeJson(jsonPayload, payload);

    // handle deserialization error
    if (error) {
        publish(topic, "{\"error\": \"payload could not be parsed\"}");
    }

    // call all callbacks that match the topic
    for (size_t i = 0; i < _numSubs; i++) {
        if (_subs[i].filter == topic) {
            _subs[i].callback(topic, jsonPayload);
        }
    }
}
