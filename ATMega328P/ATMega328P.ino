/*
    ATmega328P stepper actuator with I2C control
    - Timer1 in CTC mode, prescaler 8
    - COMPA: start STEP pulse each period (one per microstep)
    - COMPB: end STEP pulse after STEP_PULSE_WIDTH_US
    - I2C receive callback only buffers data; planning happens in loop()
*/

#include <Wire.h>
#include <avr/interrupt.h>
#include <math.h>

// --------- Configurable per actuator ----------
#define I2C_ADRESS 5
#define GEAR_RATIO 118

// Pin definitions (Arduino UNO)
#define STEP 10   // PB2 (also OC1B)
#define DIR   9   // PB1 (also OC1A)
#define MS1  13
#define MS2  12
#define MS3  11

// Fast direct-port access for STEP (PB2)
#define STEP_PORT PORTB
#define STEP_BIT  PB2

// Driver timing
#define STEP_PULSE_WIDTH_US 4     // adjust for your driver (A4988≥1µs, DRV8825≈2µs)
#define DIR_SETUP_US        2     // time from DIR change to first STEP rising edge

// --------- Motion state ----------
volatile uint32_t stepsRemaining   = 0;   // microsteps left
volatile uint16_t periodTicks      = 0;   // Timer1 ticks per microstep period
volatile uint16_t pulseTicks       = 0;   // ticks to hold STEP high

// Planning inputs (updated from I2C ISR, consumed in loop)
volatile bool     planPending      = false;
volatile float    rx_delta_angle   = 0.0f; // degrees
volatile float    rx_delta_time    = 0.0f; // seconds

// Status (for host queries)
float  delta_angle                 = 0.0f;
float  delta_time                  = 0.0f;
float  velocity                    = 0.0f; // deg/s
bool   microstepping_state[3]      = {LOW, LOW, LOW};

// --------- Timer setup ----------
static inline void timer1_setup_ctc_prescaler8() {
  // Prescaler 8 => tick = 0.5 µs at 16 MHz
  cli();
  TCCR1A = 0;                 // normal I/O on OC1A/OC1B, no PWM
  TCCR1B = 0;
  TCNT1  = 0;
  // CTC mode with OCR1A as TOP
  TCCR1B |= _BV(WGM12);
  // Prescaler 8
  TCCR1B |= _BV(CS11);
  // Do not enable interrupts yet; they are enabled when a motion starts
  TIMSK1 &= ~(_BV(OCIE1A) | _BV(OCIE1B));
  sei();
}

static inline void motion_stop() {
  // Disable Timer1 interrupts, force STEP low
  TIMSK1 &= ~(_BV(OCIE1A) | _BV(OCIE1B));
  STEP_PORT &= ~(1 << STEP_BIT);
  stepsRemaining = 0;
}

static inline void motion_start(uint32_t steps, uint16_t period_us) {
  if (steps == 0) { motion_stop(); return; }

  // Compute ticks for prescaler=8 (0.5us per tick)
  uint32_t pTicks = (uint32_t)period_us * 2u;     // >= 3200 with your 1600us min
  uint16_t wTicks = (uint16_t)max(1u, (uint32_t)STEP_PULSE_WIDTH_US * 2u);

  if (pTicks <= wTicks + 1) {
    // Ensure at least 1 tick margin
    pTicks = wTicks + 2;
  }

  cli();
  periodTicks    = (uint16_t)min(pTicks, 65535u);
  pulseTicks     = wTicks;
  stepsRemaining = steps;

  // Set period (TOP) and pulse end compare
  OCR1A = periodTicks - 1u;   // timer resets at this value
  OCR1B = pulseTicks;         // end of high pulse within the period

  // Reset counter so period starts now
  TCNT1 = 0;

  // Enable interrupts: A = start pulse, B = end pulse
  TIMSK1 |= _BV(OCIE1A) | _BV(OCIE1B);
  sei();
}

// --------- ISRs ---------
// Start of each microstep period: raise STEP, schedule COMPB to lower it
ISR(TIMER1_COMPA_vect) {
  if (stepsRemaining == 0) {
    motion_stop();
    return;
  }
  // Rising edge
  STEP_PORT |= (1 << STEP_BIT);
  stepsRemaining--;
  // OCR1B already set to pulseTicks relative to start-of-period (TCNT1=0)
}

// End of pulse: lower STEP
ISR(TIMER1_COMPB_vect) {
  STEP_PORT &= ~(1 << STEP_BIT);
}

// --------- Microstepping & planning ----------
static inline void setMicrosteppingFromDivider(int divider){
  if (divider == 1){
    microstepping_state[0] = LOW;  microstepping_state[1] = LOW;  microstepping_state[2] = LOW;
  } else if(divider == 2){
    microstepping_state[0] = HIGH; microstepping_state[1] = LOW;  microstepping_state[2] = LOW;
  } else if(divider == 4){
    microstepping_state[0] = LOW;  microstepping_state[1] = HIGH; microstepping_state[2] = LOW;
  } else if(divider == 8){
    microstepping_state[0] = HIGH; microstepping_state[1] = HIGH; microstepping_state[2] = LOW;
  } else { // 16
    microstepping_state[0] = HIGH; microstepping_state[1] = HIGH; microstepping_state[2] = HIGH;
  }
  digitalWrite(MS1, microstepping_state[0]);
  digitalWrite(MS2, microstepping_state[1]);
  digitalWrite(MS3, microstepping_state[2]);
}

static inline void planMovement(float d_angle, float d_time) {
  if (d_time == 0.0f) return;

  delta_angle = d_angle;
  delta_time  = d_time;
  velocity    = delta_angle / delta_time;

  // post-gearbox full-step angle and time per full step (µs)
  const float step_angle_deg   = 1.8f / GEAR_RATIO;
  const float time_one_step_us = (step_angle_deg / fabsf(velocity)) * 1e6f;

  // choose microstep divider so microstep interval >= 1600 µs
  int divider = 2;
  for (; divider < 17; divider *= 2) {
    if (time_one_step_us / (float)divider < 1600.0f) break;
  }
  divider /= 2;
  if (divider < 1)  divider = 1;
  if (divider > 16) divider = 16;

  setMicrosteppingFromDivider(divider);

  // Direction (setup before first step)
  digitalWrite(DIR, (delta_angle < 0) ? LOW : HIGH);
  delayMicroseconds(DIR_SETUP_US);

  // Steps and period
  uint32_t steps = lroundf(fabsf(delta_angle) / step_angle_deg * (float)divider);
  uint16_t period_us = (uint16_t)lroundf(time_one_step_us / (float)divider);

  motion_start(steps, period_us);
}

void I2CRecive(int count){
  if (count <= 0) return;

  uint8_t message_number = Wire.read();
  switch (message_number) {
    // recieve movement (delta_angle, delta_time)
    case 0:
        // read bytes into a buffer
        uint8_t buffer[8];
        for (uint8_t i = 0; i < 8 && Wire.available(); i++){
            buffer[i] = Wire.read();
        }
        
        // convert buffer to floats
        memcpy(&rx_delta_angle, &buffer[0], 4);
        memcpy(&rx_delta_time, &buffer[4], 4);

        // do all calculations in the loop
        planPending = true;
        
        break;
    
    // stop
    case 1:
      motion_stop();
      break;

    // ignore unknown message
    default:
      break;
  }
}

void I2CRequest() {
  // Return current velocity as 4 bytes
  Wire.write((uint8_t*)&velocity, sizeof(float));
}

// --------- Arduino setup/loop ----------
void setup() {
  pinMode(STEP, OUTPUT);
  pinMode(DIR,  OUTPUT);
  pinMode(MS1,  OUTPUT);
  pinMode(MS2,  OUTPUT);
  pinMode(MS3,  OUTPUT);
  digitalWrite(STEP, LOW);

  timer1_setup_ctc_prescaler8();

  Wire.begin(I2C_ADRESS);
  Wire.onRequest(I2CRequest);
  Wire.onReceive(I2CRecive);
}

void loop() {
  // Do the planning outside ISR context to keep I2C reliable
  if (planPending) {
    noInterrupts();
    float dA = rx_delta_angle;
    float dT = rx_delta_time;
    planPending = false;
    interrupts();
    planMovement(dA, dT);
  }
}
