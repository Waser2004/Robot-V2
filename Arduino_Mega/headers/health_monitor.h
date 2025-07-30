#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"
#include "mqtt_interface.h"


class HealthMonitor {
    public:
        HealthMonitor(Context& ctx, SensorReader& sensorReader, I2C_Interface& i2cInterface, MQTT_Interface& mqttInterface);

        void        performHealthCheck();
        static void sendHealthStatus(const String& topic, const JsonDocument& payload);

    private:

        static HealthMonitor* instance_;

        Context& context_;
        SensorReader& sensorReader_;
        I2C_Interface& i2cInterface_;
        MQTT_Interface& mqttInterface_;

        int maxHealthCheckRepeats = 3; // Number of checks to perform for each actuator before determining failure
}