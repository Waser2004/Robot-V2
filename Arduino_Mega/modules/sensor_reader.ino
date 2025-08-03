#include "sensor_reader.h"
#include "AS5600.h"
#include "context.h"

#define TCA_ADDR 0x70 // TCA9548A I2C multiplexer address

SensorReader::SensorReader(AS5600& as5600, Context& context) 
    : as5600_(as5600), context_(context) 
{}

void SensorReader::init() {
    // initialize the AS5600 sensor
    as5600_.begin();
    as5600_.setDirection(AS5600_CLOCK_WISE);
}

float SensorReader::readAngle(int actuator_index) {
    // select TCA9548 port
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(1 << actuator_index); // Select the desired port by setting the corresponding bit
    Wire.endTransmission();

    // read raw angle and convert it to degrees
    float angle = as5600_.rawAngle() * 360.0 / 4096;

    // normalize angle between 0° and 360°
    angle = fmod(angle - context_.actuator_zero_pos[actuator_index], 360);
    if (angle < 0) {angle += 360;}

    return angle;
}