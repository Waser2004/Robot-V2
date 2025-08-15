#include <ArduinoJson.h>

#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"
#include "health_monitor.h"
#include "mqtt_interface.h"

HealthMonitor* HealthMonitor::instance_ = nullptr;

HealthMonitor::HealthMonitor(Context& ctx, SensorReader& sensorReader, I2C_Interface& i2cInterface, MQTT_Interface& mqttInterface)
    : context_(ctx), sensorReader_(sensorReader), i2cInterface_(i2cInterface), mqttInterface_(mqttInterface) {
        instance_ = this;
    }

void HealthMonitor::performHealthCheck() {
    /* 
        This function initiates a small rotation for each actuator and verifies sensor feedback to ensure proper operation.

        It detects and categorizes possible failures:
        - [code 400] Sensor failure: Sensor readings remain at 0.0 after rotation is initiated, indicating sensor malfunction.
        - [code 401] Motor failure: Sensor readings are non-zero but do not change after rotation is initiated, indicating motor malfunction.

        If no issues are detected the actuator is assigned a good health status [code 200].
    */
    // reading storage
    float initial_readings[6];
    float current_readings[6];

    // read initial actuator angles
    for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
        initial_readings[actuator_index] = sensorReader_.readAngle(actuator_index);
    }

    // loop variables
    int  check_count = 0;
    int  complete_count = 0;
    bool health_check_complete[6] = {false, false, false, false, false, false};

    // initiate health check loop
    while (check_count < maxHealthCheckRepeats && complete_count < 6) {
        Serial.print("Health check iteration ");
        Serial.println(check_count + 1);
        // send movement request to all actuators where health check is not yet complete
        for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
            if (!health_check_complete[actuator_index]) {
                i2cInterface_.sendMovement(actuator_index, 1.0, 0.2);
            }
        }

        // await movement to complete
        delay(200);

        // read current actuator angles where health check is not yet complete
        for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
            if (!health_check_complete[actuator_index]) {
                current_readings[actuator_index] = sensorReader_.readAngle(actuator_index);
            }
        }

        // evaluate health check results
        for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
            // check if the actuator offset between initial and current readings is significant if so treat as health check complete and succesfull
            if (
                !health_check_complete[actuator_index] 
                && abs(current_readings[actuator_index] - initial_readings[actuator_index]) > 0.8
            ) {
                health_check_complete[actuator_index] = true;
                complete_count++;
            }
        }

        check_count++;
    }

    // assign health check results to context
    for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
        if (!health_check_complete[actuator_index]) {
            // likely a sensor failure
            if (initial_readings[actuator_index] == 0.0) {
                context_.health_check_results[actuator_index] = 400;
            } 
            // likely a motor failure
            else {
                context_.health_check_results[actuator_index] = 401;
            }
        } else {
            // good health check
            context_.health_check_results[actuator_index] = 200;
        }
        
        float deltaRotation = current_readings[actuator_index] - initial_readings[actuator_index];
        i2cInterface_.sendMovement(actuator_index, deltaRotation, 0.6);
    }
    
    // overall health check result
    if (complete_count == 6) {
        context_.good_health_check = true;  // all good
    } else {
        context_.good_health_check = false; // at least one actuator failed
    }
}

void HealthMonitor::sendHealthStatus(const String& topic, const JsonDocument& payload) {
    /*
        This function sends the health check results to the MQTT broker.
        It publishes a JSON object containing the health check results for each actuator.
    */
    if (!instance_) return;

    // create Json document
    JsonDocument doc;
    doc["0"] = instance_->context_.health_check_results[0];
    doc["1"] = instance_->context_.health_check_results[1];
    doc["2"] = instance_->context_.health_check_results[2];
    doc["3"] = instance_->context_.health_check_results[3];
    doc["4"] = instance_->context_.health_check_results[4];
    doc["5"] = instance_->context_.health_check_results[5];

    // serialize Json document
    String jsonString;
    serializeJson(doc, jsonString);

    // publish health status
    instance_->mqttInterface_.publish("arduino/out/health/status", jsonString);
}