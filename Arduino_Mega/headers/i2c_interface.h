#pragma once
#include <Arduino.h>

class I2C_Interface {
    public:
        // initialize I2C
        void init();

        // send functions
        void sendStop();
        void sendMovement(byte adress, float deltaRotation, float deltaTime);

        // request functions
        float requestVelocity(byte address);
};