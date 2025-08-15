/*
    The Context manages variables shared between different modules.
*/
#pragma once
#include <Arduino.h>

struct Context {
    // force stop
    bool  force_stop                     = false;

    // last checkup time
    unsigned long lastCheckupSend        = 0;
    unsigned long lastCheckupReceive     = 0;

    // healthcheck variables
    bool  good_health_check              = false;
    byte  health_check_results[6];

    float max_actuator_velocity[6]       = {6.696, 6.696, 6.696, 9.534, 6.696, 9.534}; // max velocity of each actuator in deg/s

    // movement target
    bool  execute_movement               = false; // true if a movement is currently being executed
    float current_rotation[6];
    bool  current_gripper_state          = true; // true = open, false = closed

    float target_rotation[6];
    float target_rotation_buffer[20][6];
    int   target_rotation_buffer_amount  = 0; // amount of rotations in the buffer
    

    // actuator angles
    float actuator_zero_pos[6]           = {57.5f, 238.9746, 28.125, 147.832, 353.3203, 3.515625}; // raw sensor value that corresponds to actuator 0 rotation
    float actuator_resting_pos[6]        = {0, 0, 0, 90, 0, 90}; // positino robot moves to at shutdown
    float actuator_home_pos[6]           = {0, 0, 0, 0, 0, 0}; // positino robot moves to after startup (to check if cube is on target)

    // settings
    float actuator_rotation_tolerance    = 0.1; // min offset between real and target rotation in degrees per actuator to be considered as "reached"
};