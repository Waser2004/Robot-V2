#include "mqtt_interface.h"
#include <Arduino.h>

// Constructor implementation
MQTT_Interface::MQTT_Interface(Stream& transport, size_t maxSubs = 10) 
    : _serial(transport), _maxSubs(maxSubs)
{
    _sub = new Sub[_maxSubs];
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
    // check if message starts with "pub" or "sub"
    if (message.startsWith(F("sub "))) {return}

    // get space positions
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);

    // check if message is valid format
    if (firstSpace < 0 || secondSpace < 0) {return}

    // extract topic and payload
    String topic = message.substring(firstSpace + 1, secondSpace);
    String payload = message.substring(secondSpace + 1);

    // call all callbacks that match the topic
    for (size_t i = 0; i < _numSubs; i++) {
        if (_subs[i].filter == topic) {
            _subs[i].callback(topic, payload);
        }
    }
}
