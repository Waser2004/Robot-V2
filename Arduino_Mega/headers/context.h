/*
    The Context manages variables shared between different modules.
*/
#pragma once
#include <Arduino.h>

struct Context {
    // force stop
    bool  force_stop = false;

    // healthcheck variables
    bool  good_health_check = false;
    byte  health_check_results[6];

    // movement target
    float target_rotation[6];
    bool  target_gripper_state; // true = open, false = closed

    // actuator angles
    float actuator_zero_pos[6]    = {0, 0, 0, 0, 0, 0}; // raw sensor value that corresponds to actuator 0 rotation
    float actuator_resting_pos[6] = {0, 0, 0, 90, 0, 90}; // positino robot moves to at shutdown
    float actuator_home_pos[6]    = {0, 0, 0, 0, 0, 0}; // positino robot moves to after startup (to check if cube is on target)
}