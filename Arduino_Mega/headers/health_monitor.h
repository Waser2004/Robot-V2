#pragma once

#include <Arduino.h>
#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"


class HealthMonitor {
    public:
        HealthMonitor(Context& ctx, SensorReader& sensorReader);

        void performHealthCheck();
        void sendHealthStatus();

    private:

        Context& context_;
        SensorReader& sensorReader_;
        I2C_Interface& i2cInterface_;

        int maxHealthCheckRepeats = 3; // Number of checks to perform for each actuator before determining failure
}