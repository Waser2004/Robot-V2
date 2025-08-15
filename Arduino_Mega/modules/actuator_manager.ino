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
        // calculate delta rotation in valid range (handle wrap-around for circular angles)
        float current = actuator_angles[actuator_i];
        float target = context_.target_rotation[actuator_i];
        float diff = target - current;
        // Normalize to [-180, 180]
        if (diff > 180.0f) {
            diff -= 360.0f;
        } else if (diff < -180.0f) {
            diff += 360.0f;
        }
        delta_rotation[actuator_i] = diff;

        if (actuator_i == 0) {
            Serial.print("Actuator ");
            Serial.print(actuator_i);
            Serial.print(": Current = ");
            Serial.print(current);
            Serial.print(", Target = ");
            Serial.print(target);
            Serial.print(", Delta = ");
            Serial.println(delta_rotation[actuator_i]);
        }

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
        then call loop() to (re)start movement.
    */
    // check if instance is initialized
    if (!instance_) return;

    JsonDocument responsePayload;
    String       JsonString;
    
    // add as new rotation target if override is set or no movement is currently being executed
    if (payload["override"].as<bool>() || !instance_->context_.execute_movement) {
        instance_->context_.target_rotation[0] = payload["0"].as<float>();
        instance_->context_.target_rotation[1] = payload["1"].as<float>();
        instance_->context_.target_rotation[2] = payload["2"].as<float>();
        instance_->context_.target_rotation[3] = payload["3"].as<float>();
        instance_->context_.target_rotation[4] = payload["4"].as<float>();
        instance_->context_.target_rotation[5] = payload["5"].as<float>();

        instance_->context_.target_rotation_buffer_amount = 0;
    }
    // add to target rotation buffer if buffer is not full
    else if (instance_->context_.target_rotation_buffer_amount < 20) {
        // add to target rotation buffer
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][0] = payload["0"].as<float>();
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][1] = payload["1"].as<float>();
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][2] = payload["2"].as<float>();
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][3] = payload["3"].as<float>();
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][4] = payload["4"].as<float>();
        instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][5] = payload["5"].as<float>();

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
    instance_->loop();
}

void ActuatorManager::onPathRecieve(const String& topic, const JsonDocument& payload) {
    /* 
        Handle incoming rotation/path entry: apply immediately if override or idle,
        else append to buffer (max 20). Publish buffer status, set execute_movement,
        then call loop() to (re)start movement.
    */
    // check if instance is initialized
    if (!instance_) return;

    JsonDocument responsePayload;
    String       JsonString;

    
    JsonArray path          = payload["path"].as<JsonArray>();
    bool      addToTarget   = payload["override"].as<bool>() || !instance_->context_.execute_movement;
    int       overflowCount = 0;

    // loop over all targets in the path
    for (JsonObject target : path) {

        // add as new rotation target if override is set or no movement is currently being executed
        if (addToTarget) {
            instance_->context_.target_rotation[0] = target["0"].as<float>();
            instance_->context_.target_rotation[1] = target["1"].as<float>();
            instance_->context_.target_rotation[2] = target["2"].as<float>();
            instance_->context_.target_rotation[3] = target["3"].as<float>();
            instance_->context_.target_rotation[4] = target["4"].as<float>();
            instance_->context_.target_rotation[5] = target["5"].as<float>();

            addToTarget = false;
            instance_->context_.target_rotation_buffer_amount = 0;
        }
        // add to target rotation buffer if buffer is not full
        else if (instance_->context_.target_rotation_buffer_amount < 20) {
            // add to target rotation buffer
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][0] = target["0"].as<float>();
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][1] = target["1"].as<float>();
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][2] = target["2"].as<float>();
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][3] = target["3"].as<float>();
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][4] = target["4"].as<float>();
            instance_->context_.target_rotation_buffer[instance_->context_.target_rotation_buffer_amount][5] = target["5"].as<float>();

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
    instance_->loop();
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
    mqtt_interface_.publish("arduino/confirm/rotation/clear", "{}");
}