#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"

// Constructor implementation
MQTT_Interface::MQTT_Interface(Stream& transport, Context& context, size_t maxSubs) 
    : serial_(transport), context_(context), maxSubs_(maxSubs)
{
    subs_ = new Sub[maxSubs_];
    instance_ = this;
}

// checkup methods
void MQTT_Interface::onCheckupReceive(const String& topic, const JsonDocument& payload) {
    /*
        This method is called when a checkup message is received.
        It updates the last checkup receive time.
    */
    if (!instance_) return;
    instance_.context_.lastCheckupReceive = millis();
}

void MQTT_Interface::sendCheckup() {
    /*
        This method continually sends a checkup message every second.
    */
    // check if one second has passed since last checkup
    if (millis() - context_.lastCheckupSend < 1000) {
        return;
    }

    // publish checkup
    publish("checkup", "{}");
    
    // update last checkup send time
    context_.lastCheckupSend = millis();
}

// Publish method implementation
bool MQTT_Interface::publish(const String& topic, const String& payload) {
    // pub payload
    serial_.print(F("pub "));
    serial_.print(topic);
    serial_.print(" ");
    serial_.println(payload);

    // pub complete return true
    return true;
}

// Subscribe method implementation
bool MQTT_Interface::subscribe(const String& filter, const MQTT_Callback& callback) {
    // check if subscriptions are full
    if (numSubs_ >= maxSubs_) {
        return false;
    }

    // add subscription
    subs_[numSubs_].filter = filter;
    subs_[numSubs_].callback = callback;
    numSubs_++;

    // subscription complete return true
    return true;
}

void MQTT_Interface::loop() {
    // message handling loop
    while (serial_.available()) {
        //read char
        char c = (char)serial_.read();

        // end of line
        if (c == '\n') {
            // move input from buffer to message variable
            String message = inputBuffer_;
            inputBuffer_ = "";

            // handle message
            message.trim();
            if (message.length()) {
                handleMessage(message);
            }
        }
        // add char to input buffer
        else {
            inputBuffer_ += c;
        }
    }

    // send checkup
    sendCheckup();

    // Check if last checkup was received more than 3 seconds ago if so come to an emergency stop
    if (millis() - context_.lastCheckupReceive > 3000) {
        context_.force_stop = true;
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
    for (size_t i = 0; i < numSubs_; i++) {
        if (subs_[i].filter == topic) {
            subs_[i].callback(topic, jsonPayload);
        }
    }
}
