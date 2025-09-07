#include <ArduinoJson.h>

#include "context.h"
#include "sensor_reader.h"
#include "i2c_interface.h"
#include "mqtt_interface.h"
#include "actuator_manager.h"

ActuatorManager* ActuatorManager::instance_ = nullptr;

ActuatorManager::ActuatorManager(Context& context, SensorReader& sensorReader, I2C_Interface& i2cInterface, MQTT_Interface& mqttInterface)
    : mqtt_interface_(mqttInterface), 
      context_(context), 
      sensorReader_(sensorReader), 
      i2c_interface_(i2cInterface) 
{
    instance_ = this;
}

void ActuatorManager::loop() {
    /*
        This function is called in the main loop.
        If a movement is currently being executed, it calls executeRotation() to continue the movement.
        if the target is reached, it checks if there are more targets in the buffer and sets the next target.
        If the buffer is empty, it stops the movement.
    */
    bool targetReached = executeRotation();

    if (targetReached) {
        if (context_.target_rotation_buffer_amount > 0) {
            // load next target from buffer[0]
            for (int i = 0; i < 6; ++i) {
                context_.target_rotation[i] = context_.target_rotation_buffer[0][i];
            }

            // shift remaining buffered targets up by one
            for (int i = 1; i < context_.target_rotation_buffer_amount; ++i) {
                for (int j = 0; j < 6; ++j) {
                    context_.target_rotation_buffer[i - 1][j] = context_.target_rotation_buffer[i][j];
                }
            }

            // reduce count
            context_.target_rotation_buffer_amount--;

            // start movement for the new target
            executeRotation();
        } else {
            context_.execute_movement = false;
        }

        JsonDocument payload;
        String       JSONString;
        payload["buffer-amount"] = context_.target_rotation_buffer_amount;
        serializeJson(payload, JSONString);

        mqtt_interface_.publish("arduino/out/rotation/target/reached", JSONString);
    }
}

bool ActuatorManager::executeRotation() {
    /*
        This function handles the actuator movement.    
        If a rotation target is set, this function will continuesly read the sensors and update the actuators delta values.

        Returns:
            bool: true if target is reached, false otherwise
    */
    // get current actuator angles
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        context_.current_rotation[actuator_i] = sensorReader_.readAngle(actuator_i);
    }

    float delta_rotation[6];
    float min_execution_time = 0.0f;
    bool  target_reached = true;

    // calculate delta rotation
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        // calculate delta rotation in valid range (handle wrap-around for circular angles)
        float current = context_.current_rotation[actuator_i];
        float target = context_.target_rotation[actuator_i];
        float diff = target - current;
        // Normalize to [-180, 180]
        if (diff > 180.0f) {
            diff -= 360.0f;
        } else if (diff < -180.0f) {
            diff += 360.0f;
        }
        // handle deadzone
        diff = handleDeadzone(actuator_i, current, diff);
        delta_rotation[actuator_i] = diff;

        // target reached if delta is smaller than tolerance
        if (abs(delta_rotation[actuator_i]) > context_.actuator_rotation_tolerance) {
            target_reached = false;
        } 

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

void ActuatorManager::onTargetRecieve(const String& topic, const JsonDocument& payload) {
    /*
        Handle incoming single rotation target: apply immediately if override or idle,
        else append to buffer (max 20). Publish buffer status, set execute_movement,
        then call executeRotation() to (re)start movement.
    */
    // check if instance is initialized
    if (!instance_) return;

    JsonDocument responsePayload;
    String       JsonString;
    
    // add as new rotation target if override is set or no movement is currently being executed
    if (payload["override"].as<bool>() || !instance_->context_.execute_movement) {
        instance_->context_.target_rotation[0] = instance_->normalizeAngle(payload["0"].as<float>());
        instance_->context_.target_rotation[1] = instance_->normalizeAngle(payload["1"].as<float>());
        instance_->context_.target_rotation[2] = instance_->normalizeAngle(payload["2"].as<float>());
        instance_->context_.target_rotation[3] = instance_->normalizeAngle(payload["3"].as<float>());
        instance_->context_.target_rotation[4] = instance_->normalizeAngle(payload["4"].as<float>());
        instance_->context_.target_rotation[5] = instance_->normalizeAngle(payload["5"].as<float>());

        instance_->context_.target_rotation_buffer_amount = 0;
    }
    // add to target rotation buffer if buffer is not full
    else if (instance_->context_.target_rotation_buffer_amount < instance_->context_.target_rotation_buffer_size) {
        // add to target rotation buffer
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][0] = instance_->normalizeAngle(payload["0"].as<float>());
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][1] = instance_->normalizeAngle(payload["1"].as<float>());
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][2] = instance_->normalizeAngle(payload["2"].as<float>());
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][3] = instance_->normalizeAngle(payload["3"].as<float>());
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][4] = instance_->normalizeAngle(payload["4"].as<float>());
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][5] = instance_->normalizeAngle(payload["5"].as<float>());

        // increment buffer amount
        instance_->context_.target_rotation_buffer_amount ++;
    } 
    // buffer is full, add buffer overflow to response payload
    else {
        responsePayload["buffer-overflow"] = 1;
    }

    // confirm target rotation recieved
    responsePayload["buffer-amount"] = instance_->context_.target_rotation_buffer_amount;
    serializeJson(responsePayload, JsonString);

    instance_->mqtt_interface_.publish("arduino/confirm/rotation/target", JsonString);

    // set execute movement flag and send acutator messages
    instance_->context_.execute_movement = true;
    instance_->executeRotation();
}

void ActuatorManager::onPathRecieve(const String& topic, const JsonDocument& payload) {
    /* 
        Handle incoming rotation/path entry: apply immediately if override or idle,
        else append to buffer (max 20). Publish buffer status, set execute_movement,
        then call executeRotation() to (re)start movement.
    */
    // check if instance is initialized
    if (!instance_) return;

    JsonDocument responsePayload;
    String       JsonString;

    JsonArrayConst path          = payload["path"].as<JsonArrayConst>();
    bool           addToTarget   = payload["override"].as<bool>() || !instance_->context_.execute_movement;
    int            overflowCount = 0;

    // loop over all targets in the path
    for (JsonObjectConst target : path) {

        // add as new rotation target if override is set or no movement is currently being executed
        if (addToTarget) {
            instance_->context_.target_rotation[0] = instance_->normalizeAngle(target["0"].as<float>());
            instance_->context_.target_rotation[1] = instance_->normalizeAngle(target["1"].as<float>());
            instance_->context_.target_rotation[2] = instance_->normalizeAngle(target["2"].as<float>());
            instance_->context_.target_rotation[3] = instance_->normalizeAngle(target["3"].as<float>());
            instance_->context_.target_rotation[4] = instance_->normalizeAngle(target["4"].as<float>());
            instance_->context_.target_rotation[5] = instance_->normalizeAngle(target["5"].as<float>());

            addToTarget = false;
            instance_->context_.target_rotation_buffer_amount = 0;
        }
        // add to target rotation buffer if buffer is not full
        else if (instance_->context_.target_rotation_buffer_amount < instance_->context_.target_rotation_buffer_size) {
            // add to target rotation buffer
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][0] = instance_->normalizeAngle(target["0"].as<float>());
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][1] = instance_->normalizeAngle(target["1"].as<float>());
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][2] = instance_->normalizeAngle(target["2"].as<float>());
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][3] = instance_->normalizeAngle(target["3"].as<float>());
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][4] = instance_->normalizeAngle(target["4"].as<float>());
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][5] = instance_->normalizeAngle(target["5"].as<float>());

            // increment buffer amount
            instance_->context_.target_rotation_buffer_amount ++;
        } 
        // buffer is full, add buffer overflow to response payload
        else {
            overflowCount ++;
        }
    }
    
    // prepare response payload
    if (overflowCount > 0) {
        responsePayload["buffer-overflow"] = overflowCount;
    }
    responsePayload["buffer-amount"] = instance_->context_.target_rotation_buffer_amount;
    serializeJson(responsePayload, JsonString);

    instance_->mqtt_interface_.publish("arduino/confirm/rotation/path", JsonString);

    // set execute movement flag and send acutator messages
    instance_->context_.execute_movement = true;
    instance_->executeRotation();
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
        response[String(actuator_i)] = instance_->sensorReader_.readAngle(actuator_i);
    }

    // serialize and publish current angles
    serializeJson(response, JsonString);
    instance_->mqtt_interface_.publish("arduino/out/rotation/current", JsonString);


    response.clear();
    // request current velocities from actuators
    for (int actuator_i = 0; actuator_i < 6; actuator_i++) {
        float velocity = instance_->i2c_interface_.requestVelocity(actuator_i);
        response[String(actuator_i)] = velocity;
    }

    // serialize and publish current velocities
    serializeJson(response, JsonString);
    instance_->mqtt_interface_.publish("arduino/out/rotation/velocity", JsonString);

    // create target angle json
    response.clear();
    response["0"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[0] : JsonVariant();
    response["1"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[1] : JsonVariant();
    response["2"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[2] : JsonVariant();
    response["3"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[3] : JsonVariant();
    response["4"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[4] : JsonVariant();
    response["5"] = instance_->context_.execute_movement ? instance_->context_.target_rotation[5] : JsonVariant();

    // serialize and publish target angles
    serializeJson(response, JsonString);
    instance_->mqtt_interface_.publish("arduino/out/rotation/target", JsonString);

    // create target rotation buffer json
    response.clear();
    response["buffer-amount"] = instance_->context_.target_rotation_buffer_amount;

    // serialize and publish target rotation buffer
    serializeJson(response, JsonString);
    instance_->mqtt_interface_.publish("arduino/out/rotation/buffer", JsonString);
}

void ActuatorManager::onRotationClear(const String& topic, const JsonDocument& payload) {
    /*
        This function is called when the rotation should be cleared.
        It clears the target rotation buffer and resets the target rotation.
    */
    // check if instance is initialized
    if (!instance_) return;

    // halt movement and reset buffer size
    instance_->context_.execute_movement = false;
    instance_->context_.target_rotation_buffer_amount = 0;

    // confirm rotation clear
    instance_->mqtt_interface_.publish("arduino/confirm/rotation/clear", "{}");
}

float ActuatorManager::normalizeAngle(float angle) {
    /*
        Normalize angle to [0, 360]
    */
    angle = fmod(angle, 360);
    if (angle < 0) {angle += 360;}
    return angle;
}

float ActuatorManager::handleDeadzone(int actuator_i, float angle, float delta) {
    /*
        Handle deadzone for actuator.
        if the delta angle would move through the deadzone the rotation direction is reversed
    */
    // calculate raw distance to deadzone edges
    float dtod_start = context_.actuator_deadzone_start[actuator_i] - angle;
    float dtod_end   = context_.actuator_deadzone_end[actuator_i] - angle;

    // normalize to [-180, 180]
    if (dtod_start > 180.0f) {
        dtod_start -= 360.0f;
    } else if (dtod_start < -180.0f) {
        dtod_start += 360.0f;
    }
    if (dtod_end > 180.0f) {
        dtod_end -= 360.0f;
    } else if (dtod_end < -180.0f) {
        dtod_end += 360.0f;
    }

    // convert dtods to be relative to positive delta
    float delta_sign = delta < 0 ? -1.0f : 1.0f;
    delta      *= delta_sign;
    dtod_start *= delta_sign;
    dtod_end   *= delta_sign;

    // return reversed direction if deadzone is crossed
    if (0 < dtod_start && dtod_start < delta || 0 < dtod_end && dtod_end < delta) {
        return - (360.0f - delta) * delta_sign;
    }
    // return original delta if no deadzone is crossed
    return delta * delta_sign;
}
