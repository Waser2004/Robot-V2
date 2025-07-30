#include "health_monitor.h"
#include "sensor_reader.h"
#include "i2c_interface.h"

HealthMonitor::HealthMonitor(Context& ctx, SensorReader& sensorReader, I2C_Interface& i2cInterface)
    : context_(ctx), sensorReader_(sensorReader), i2cInterface_(i2cInterface) {}

bool HealthMonitor::performHealthCheck() {
    /* 
        This function initiates a small rotation for each actuator and verifies sensor feedback to ensure proper operation.

        It detects and categorizes possible failures:
        - [code 0] Sensor failure: Sensor readings remain at 0.0 after rotation is initiated, indicating sensor malfunction.
        - [code 1] Motor failure: Sensor readings are non-zero but do not change after rotation is initiated, indicating motor malfunction.

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
        // send movement request to all actuators where health check is not yet complete
        for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
            if (!health_check_complete[actuator_index]) {
                i2cInterface_.sendMovement(actuator_index, 1.0, 0.2);
            }
        }

        // await movement to complete
        delayMicroseconds(200000);
        i2cInterface_.sendStop(); // TODO: actuators will not stop automatically after movement complete. Until their code is updated this is needed

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
    }

    // assign health check results to context
    for (int actuator_index = 0; actuator_index < 6; actuator_index++) {
        if (!health_check_complete[actuator_index]) {
            // likely a sensor failure
            if (initial_readings[actuator_index] == 0.0) {
                context_.health_check_results[actuator_index] = 0;
            } 
            // likely a motor failure
            else {
                context_.health_check_results[actuator_index] = 1;
            }
        } else {
            // good health check
            context_.health_check_results[actuator_index] = 200;
        }
        
        float deltaRotation = current_readings[actuator_index] - initial_readings[actuator_index];
        i2cInterface_.sendMovement(actuator_index, deltaRotation, 0.6);
    }

    // await rotation back
    delayMicroseconds(600000);
    i2cInterface_.sendStop(); // TODO: actuators will not stop automatically after movement complete. Until their code is updated this is needed
    
    // overall health check result
    if (complete_count == 6) {
        context_.good_health_check = true;  // all good
    } else {
        context_.good_health_check = false; // at least one actuator failed
    }
}

HealthMonitor::sendHealthStatus() {
    // create payload string
    String payload = "{\"result\":[";

    for (int i = 0; i < 6; i++) {
        payload += String(context_.health_check_results[i]);
        if (i < 5) payload += ",";
    }
    payload += "]}";

    // publish Health status
    Serial1.write("pub arduino/out/health/status " + payload + "\n");
}