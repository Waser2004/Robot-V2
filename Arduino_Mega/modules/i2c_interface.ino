#include <Wire.h>
#include "i2c_interface.h"

void I2C_Interface::init() {
    /* This function initializes the I2C communication */
    Wire.begin();
    Wire.setClock(400000); // Set I2C clock speed to 400kHz
}

void I2C_Interface::sendStop() {
    /* This function sends the stop signal to all actuators */
    byte message = 1;

    for (int i = 0; i < 6; i++) {
        Wire.beginTransmission(i);
        Wire.write(message);
        Wire.endTransmission();
    }
}

void I2C_Interface::sendMovement(byte address, float deltaRotation, float deltaTime) {
    /* 
        This function send a Movement to one Actuator
        
        Args:
            address: the I2C address of the actuator (actuator index)
            deltaRotation: the roation in degrees for the movement
            deltaTime: the time in seconds for the movement
    */
    // create message
    byte message[9];


    message[0] = 0;
    memcpy(&message[1], &deltaRotation, sizeof(float));
    memcpy(&message[5], &deltaTime, sizeof(float));

    // convert to floats
    float delta_angle;
    float delta_time;
    memcpy(&delta_angle, &message[1], 4);
    memcpy(&delta_time,  &message[5], 4);

    // send message
    Wire.beginTransmission(address);
    Wire.write(message, sizeof(message));
    Wire.endTransmission();
}

float I2C_Interface::requestVelocity(byte address) {
    /* 
        This function requests the current velocity of an actuator
    */
    // request one float
    Wire.requestFrom(address, sizeof(float));
    
    if (Wire.available() == sizeof(float)) {
        // read bytes into buffer
        byte buffer[sizeof(float)];
        for (int i = 0; i < sizeof(float); i++) {
            buffer[i] = Wire.read();
        }

        // convert bytes to float
        float velocity;
        memcpy(&velocity, buffer, sizeof(float));
        return velocity;
    }
    
    return 0.0f; // Return 0 if no data is available
}