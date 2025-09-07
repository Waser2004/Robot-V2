#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "mqtt_interface.h"

MQTT_Interface* MQTT_Interface::instance_ = nullptr;

// Constructor implementation
MQTT_Interface::MQTT_Interface(Stream& transport, Context& context, size_t maxSubs) 
    : serial_(transport), context_(context), maxSubs_(maxSubs)
{
    subs_ = new Sub[maxSubs_];
    instance_ = this;
}

// Flow control thresholds for Serial1 RX on Mega
static const int RX_HIGH_WATERMARK = 48; // send XOFF at/above this
static const int RX_LOW_WATERMARK  = 16; // send XON at/below this

void MQTT_Interface::handleFlowControl() {
    // Monitor RX pending bytes and emit XOFF/XON to ESP-01
    int avail = serial_.available();
    if (!xoffSent_ && avail >= RX_HIGH_WATERMARK) {
        serial_.write((uint8_t)0x13); // XOFF
        xoffSent_ = true;
    } else if (xoffSent_ && avail <= RX_LOW_WATERMARK) {
        serial_.write((uint8_t)0x11); // XON
        xoffSent_ = false;
    }
}

// checkup methods
void MQTT_Interface::onCheckupReceive(const String& topic, const JsonDocument& payload) {
    /*
        This method is called when a checkup message is received.
        It updates the last checkup receive time.
    */
    if (!instance_) return;
    
    instance_->context_.lastCheckupReceive = millis();
    instance_->context_.force_stop         = false;
}

void MQTT_Interface::sendCheckup() {
    /*
        This method continually sends a checkup message every second.
    */
    // check if one second has passed since last checkup
    if (millis() - context_.lastCheckupSend < 1000) {
        return;
    }

    // create checkup payload
    JsonDocument checkupPayload;
    checkupPayload["0"] = context_.current_rotation[0];
    checkupPayload["1"] = context_.current_rotation[1];
    checkupPayload["2"] = context_.current_rotation[2];
    checkupPayload["3"] = context_.current_rotation[3];
    checkupPayload["4"] = context_.current_rotation[4];
    checkupPayload["5"] = context_.current_rotation[5];
    checkupPayload["gripper"] = context_.current_gripper_state;

    // serialize payload to string and send
    String payload;
    serializeJson(checkupPayload, payload);
    publish("arduino/out/checkup", payload);
    
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
    // Apply flow control before draining RX (pause sender if we're backed up)
    handleFlowControl();

    // message handling loop
    while (serial_.available()) {
        // read char
        char c = serial_.read();

        // end of line
        if (c == '\n') {
            // null-terminate and handle message
            inputBuffer_[inputBufferHead_] = '\0';
            handleMessage();

            // reset buffer
            inputBufferHead_ = 0;
        }
        // add char to input buffer (guard against overflow)
        else {
            if (inputBufferHead_ + 1 < sizeof(inputBuffer_)) {
                inputBuffer_[inputBufferHead_] = c;
                inputBufferHead_++;
            } else {
                // buffer full, drop input and reset to avoid overflow
                inputBufferHead_ = 0;
            }
        }
    }

    // send checkup
    sendCheckup();

    // Check if last checkup was received more than 3 seconds ago if so come to an emergency stop
    if (millis() - context_.lastCheckupReceive > 3000) {
        context_.force_stop = true;
    }   

    // Re-evaluate flow control after draining some bytes
    handleFlowControl();
}

void MQTT_Interface::handleMessage() {
    // check if message starts with "sub " - skip subscription messages
    if (strncmp(inputBuffer_, "sub ", 4) == 0) {
        return;
    }

    // find first and second spaces
    const char* firstSpace = strchr(inputBuffer_, ' ');
    if (!firstSpace) return;
    const char* secondSpace = strchr(firstSpace + 1, ' ');
    if (!secondSpace) return;

    // extract topic into a temporary buffer
    size_t topicLen = secondSpace - (firstSpace + 1);
    if (topicLen == 0 || topicLen > 64) return; // sanity checks
    char topicBuf[64];
    memcpy(topicBuf, firstSpace + 1, topicLen);
    topicBuf[topicLen] = '\0';

    // payload starts after second space
    const char* payloadPtr = secondSpace + 1;

    // deserialize JSON payload from C-string
    // Use a dynamic doc sized reasonably for expected payloads
    StaticJsonDocument<512> jsonPayload;
    DeserializationError error = deserializeJson(jsonPayload, payloadPtr);

    // handle deserialization error
    if (error) {
        // publish error back using topicBuf
        publish(String(topicBuf), String("{\"error\": " + String(error.c_str()) + "}"));
        return;
    }

    // call all callbacks that match the topic
    for (size_t i = 0; i < numSubs_; i++) {
        if (subs_[i].filter == String(topicBuf)) {
            subs_[i].callback(String(topicBuf), jsonPayload);
        }
    }
}
