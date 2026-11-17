#ifndef BUCK_PWM_H
#define BUCK_PWM_H

#include <stdint.h>
#include <stdbool.h>

// Set up 100 kHz PWM on GPIO 40, output starts at 0% duty.
void buck_pwm_init(void);

// Ideal duty (Vout / Vin) as a PWM level, clamped to the safe range.
uint16_t duty_cycle_calc(float vin, float d_vout);

// Ramp duty from 0 up to the calculated value over ~10 ms (limits inrush).
void buck_soft_start(float vin, float d_vout);

// One step of closed-loop control. Call at a fixed rate (e.g. after every
// ADC capture). Returns true when vout is within tolerance of d_vout.
bool buck_regulate(float vin, float d_vout, float vout);

// Force duty to 0 and stop the PWM (e.g. on overcurrent).
void buck_disable(void);

#endif