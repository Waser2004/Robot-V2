#pragma once

#include <Arduino.h>
#include "AS5600.h"
#include "context.h"

class SensorReader {
    public:

        SensorReader(AS5600& as5600, Context& context);

        float readAngle(int actuator_index);

    private:

        Context& context_;
        AS5600& as5600_;
        const int TCA_ADDR = 0x70;

};