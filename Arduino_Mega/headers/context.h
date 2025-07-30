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

    float actuator_zero_pos[6] = {0, 0, 0, 0, 0, 0}; 
}