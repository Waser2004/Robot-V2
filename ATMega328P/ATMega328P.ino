/*
    This script is for the ATMega328P microcontroller to control one actuator
    using I2C communication. It calculates the required steps based on the
    desired angle and time, and controls the stepper motor accordingly.
*/

#include <Wire.h>

// These variables have to be set for each Actuator differently
#define I2C_ADRESS 0
#define GEAR_RATIO 168

// pin definintions
#define STEP 16
#define DIR  15
#define MS1  19
#define MS2  18
#define MS3  17

// Movement variables
float delta_angle             = 0.0; // in degrees
float delta_time              = 0.0; // in seconds
float velocity                = 0.0; // deg/s

int   delay_microseconds      = 0;
bool  microstepping_state[3] = {LOW, LOW, LOW};

// track steps
int   requiered_steps_amount  = 0;
int   steps_done              = 0;

// operation variables
bool stop_movement            = false;

void calculateActionVariables() {
    /*
        This function calculates the microstepping divider based on the velocity and the gear ratio
        The divider is calculated so that the time for one step time is as low as possible and
        at minimum 1600 microseconds.
    */
    // convert velocity to time for one step (in Microseconds)
    float time_for_one_step = (1.8 / GEAR_RATIO) / abs(velocity) * 1000000;

    // check microstepping
    int divider = 2;
    for (; divider < 17; divider *= 2) {
        if (time_for_one_step / float(divider) < 1600) {
            break;
        }
    }
    divider /= 2; // adjust divider to the last valid value

    // calculate requiered steps amount
    steps_done = 0;
    requiered_steps_amount = round(abs(delta_angle) / (1.8 / GEAR_RATIO)) * divider;

    // calculate delay in microseconds for one step
    delay_microseconds = round(time_for_one_step / float(divider));

    // TODO: update based on real world actuator direction
    digitalWrite(DIR, delta_angle < 0 ? LOW : HIGH);
    setMicrosteppingFromDivider(divider);
}

void setMicrosteppingFromDivider(int divider){
    /* 
        This function sets the microstepping pins based on the divider value.
        The divider value is expected to be one of the following: 1, 2, 4, 8, or 16.
    */
    // encode divider into microstepping states
    if (divider == 1){
        microstepping_state[0] = LOW;
        microstepping_state[1] = LOW;
        microstepping_state[2] = LOW;
    } else if(divider == 2){
        microstepping_state[0] = HIGH;
        microstepping_state[1] = LOW;
        microstepping_state[2] = LOW;
    } else if(divider == 4){
        microstepping_state[0] = LOW;
        microstepping_state[1] = HIGH;
        microstepping_state[2] = LOW;
    } else if(divider == 8){
        microstepping_state[0] = HIGH;
        microstepping_state[1] = HIGH;
        microstepping_state[2] = LOW;
    } else if(divider == 16){
        microstepping_state[0] = HIGH;
        microstepping_state[1] = HIGH;
        microstepping_state[2] = HIGH;
    }

    // apply microstepping states to pins
    digitalWrite(MS1, microstepping_state[0]);
    digitalWrite(MS2, microstepping_state[1]);
    digitalWrite(MS3, microstepping_state[2]);
}

void I2CRecive(int recieve_amount){
    /* This Function handles incoming messages and extract the requiered float values*/
    // if recieve amount is more than 0
    if (recieve_amount <= 0) {return;}

    // read message number
    byte message_number = Wire.read();

    switch (message_number) {
        // recieve delta angle and delta rotation
        case 0:
            // read delta angle and delta rotation floats
            byte read_buffer[8];
            for (int i = 0; i < 8; i++){
                read_buffer[i] = Wire.read();
            }

            // convert to floats
            memcpy(&delta_angle, &read_buffer[0], 4);
            memcpy(&delta_time,  &read_buffer[4], 4);

            // calculate new velocity
            if (delta_time == 0) {return;}
            velocity = delta_angle / delta_time;
            stop_movement = false;

            // calculate action variables
            calculateActionVariables();
        
            break;
        // stop
        case 1:
            stop_movement = true;
            break;
    }
}

void I2CRequest() {
    /* This Function handles requests (currently Nothing can be requested) */
    return;
}

void setup() {
    // set up I2C communications
    Wire.begin(I2C_ADRESS);

    Wire.onRequest(I2CRequest);
    Wire.onReceive(I2CRecive);

    // set pin Modes
    pinMode(STEP, OUTPUT);
    pinMode(DIR, OUTPUT);
    pinMode(MS1, OUTPUT);
    pinMode(MS2, OUTPUT);
    pinMode(MS3, OUTPUT);
}

void loop() {
    if (!stop_movement && requiered_steps_amount > steps_done) {
        // do one step
        digitalWrite(STEP, HIGH);
        delayMicroseconds(100);
        digitalWrite(STEP, LOW);
        delayMicroseconds(delay_microseconds - 100);

        // keep track of steps to know when to stop
        steps_done++;
    }
}
