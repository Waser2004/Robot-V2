#include "i2c_interface.h"

void I2C_Interface::sendStop() {
    /* This function sends the stop signal to all actuators */
    byte message = 0;

    for (int i = 0; i < 6; i++) {
        Wire.beginTransmission(i);
        Wire.write(message, sizeof(message));
        Wire.endTransmission();
    }
}

void I2C_Interface::sendMovement(byte address, float deltaRotation, float deltaTime) {
    /* This function send a Movement to one Actuator */
    // create message
    byte message[9];

    message[0] = 1;
    memcpy(&message[1], &deltaRotation, sizeof(float));
    memcpy(&message[5], &deltaTime, sizeof(float));

    // send message
    Wire.beginTransmission(address);
    Wire.write(message, sizeof(message));
    Wire.endTransmission();
}