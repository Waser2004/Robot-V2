#pragma once

#include <arduino.h>
#include <ArduinoJson.h>

#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"
#include "mqtt_interface.h"

class ActuatorManager {
    public:

        ActuatorManager(Context& context, SensorReader& sensorReader, I2C_Interface& i2cInterface, MQTT_Interface& mqttInterface);

        bool        loop();
        static void onTargetRecieve(const String& topic, const JsonDocument& payload);
        static void onActuatorInfo(const String& topic, const JsonDocument& payload)

    private:

        static ActuatorManager* instance_;

        Context& context_;
        SensorReader& sensorReader_;
        I2C_Interface& i2c_interface_;

}
