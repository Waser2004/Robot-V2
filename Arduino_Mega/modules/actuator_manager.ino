#include <ArduinoJson.h>

#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"
#include "mqtt_interface.h"
#include "actuator_manager.h"

ActuatorManager::ActuatorManager(Context& context, SensorReader& sensorReader, I2C_Interface& i2cInterface, MQTT_Interface& mqttInterface)
    : mqtt_interface_(mqttInterface), 
      context_(context), 
      sensorReader_(sensorReader), 
      i2c_interface_(i2cInterface) 
{
    instance_ = this;
}

bool ActuatorManager::loop() {
    /*
        This function manages the actuator operations in the main loop. If a rotatino target is set,
        this function will continuesly read the sensors and update the actuators delta values.

        Returns:
            bool: true if target is reached, false otherwise
    */
    // get current actuator angles
    float actuator_angles[6];
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        actuator_angles[actuator_i] = sensorReader_.readAngle(actuator_i);
    }

    float delta_rotation[6];
    float min_execution_time = 0.0f;
    bool  target_reached = true;

    // calculate delta rotation
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        // calculate delta rotation in valid range
        float current = actuator_angles[actuator_i];
        float target = context_.target_rotation[actuator_i];
        float delta_rotation[actuator_i] = max(target, current) - min(target, current);

        // target reached if delta is smaller than tolerance
        if (delta_rotation[actuator_i] > context_.actuator_rotation_tolerance) {
            target_reached = false;
        } 

        // apply delta with sign
        float sign = (target > current) ? 1.0f : (target < current) ? -1.0f : 0.0f;
        delta_rotation[actuator_i] *= sign;

        // calculate min execution time
        float execution_time = abs(delta_rotation[actuator_i]) / context_.max_actuator_velocity[actuator_i];
        if (execution_time > min_execution_time) {
            min_execution_time = execution_time;
        }
    }

    // target reached, send stop message
    if (target_reached) {
        return true;
    }

    // target not yet reached, send actuator messages
    for (int actuator_i = 0; actuator_i < 6; actuator_i++){
        i2c_interface_.sendMovement(actuator_i, delta_rotation[actuator_i], min_execution_time);
    }

    return false;
}

void ActuatorManager::onTargetRecive(const String& topic, const JsonDocument& payload) {
    /*
        This function is called when a target rotation is received via MQTT.
        It updates the target_rotation and sends the new movement to the actuators.
    */
    // check if instance is initialized
    if (!instance_) return;
    
    // extract rotation values
    instance_.context_.target_rotation[0] = payload["0"].as<float>();
    instance_.context_.target_rotation[1] = payload["1"].as<float>();
    instance_.context_.target_rotation[2] = payload["2"].as<float>();
    instance_.context_.target_rotation[3] = payload["3"].as<float>();
    instance_.context_.target_rotation[4] = payload["4"].as<float>();
    instance_.context_.target_rotation[5] = payload["5"].as<float>();

    // set execute movement flag and send acutator messages
    instance_.context_.execute_movement = true;
    instance_.loop();
}

void ActuatorManager::onActuatorInfo(const String& topic, const JsonDocument& payload) {
    /*
        This function is called when actuator information is requested via MQTT.
        It returns the current and target rotation as well as the current actuator velocities.
    */
    // check if instance is initialized
    if (!instance_) return;

    // json variables
    JsonDocument response;
    String JsonString;

    // get current actuator angles
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        response[String(actuator_i)] = instance_.sensorReader_.readAngle(actuator_i);
    }

    // serialize and publish current angles
    serializeJson(response, JsonString);
    instance_.mqtt_interface.publish("arduino/out/rotation/current", JsonString);


    response.clear();
    // request current velocities from actuators
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        float velocity = instance_.i2c_interface_.requestVelocity(actuator_i);
        response[String(actuator_i)] = velocity;
    }

    // serialize and publish current velocities
    serializeJson(response, JsonString);
    instance_.mqtt_interface.publish("arduino/out/rotation/velocity", JsonString);

    // create target angle json
    resposne.clear();
    response["0"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[0] : nullptr;
    response["1"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[1] : nullptr;
    response["2"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[2] : nullptr;
    response["3"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[3] : nullptr;
    response["4"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[4] : nullptr;
    response["5"] = instance_.context_.execute_movement ? instance_.context_.target_rotation[5] : nullptr;

    // serialize and publish target angles
    serializeJson(response, JsonString);
    instance_.mqtt_interface.publish("arduino/out/rotation/target", JsonString);
}