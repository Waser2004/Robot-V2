#pragma once
#include <Arduino.h>

class I2C_Interface {
    public:
        // message functions
        void sendStop();
        void sendMovement(byte adress, float deltaRotation, float deltaTime);
};