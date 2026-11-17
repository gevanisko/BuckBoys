#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "buck_pwm.h"

//PWM setup
#define PWM_PIN 40      //GPIO 40 = PWM slice 8, channel A on RP2350B
#define PWM_WRAP 1499    //150 MHz / (1499 + 1) = exactly 100 kHz, 1500 duty steps
#define PWM_TOP (PWM_WRAP + 1) // level for 100% duty

//Duty limits (as PWM levels). Max < 100% so a bootstrap high-side driver
//can recharge; adjust to your gate driver's datasheet.
#define DUTY_MAX ((PWM_TOP * 90) / 100)   //90%
#define DUTY_MIN 0

//Control tuning
#define VOUT_TOL 0.05f //V: "in regulation" band (within 50mV)
#define KI 5.0f //PWM counts per volt of error, per update
#define TRIM_MAX 300 //max correction on top of Vout/Vin (20% of range) (300 / 1500) essentially prevents are system from overcorrecting

static uint slice;
static uint chan;
static float trim = 0.0f; //accumulated correction (counts) from feedback

//ensures our DC wont exceed 90%
static int32_t clamp_level(int32_t level) {
    if (level < DUTY_MIN) return DUTY_MIN;
    if (level > DUTY_MAX) return DUTY_MAX;
    return level;
}

//used to set dc quickly without having to write out the entire function
static void set_level(int32_t level) {
    pwm_set_chan_level(slice, chan, (uint16_t)clamp_level(level));
}

//initalization of our buck
void buck_pwm_init(void) {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(PWM_PIN);
    chan  = pwm_gpio_to_channel(PWM_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1.0f); // run at full 150 MHz
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice, &cfg, false); // SDK pwm_init: configure, don't start

    pwm_set_chan_level(slice, chan, 0);// start at 0% duty
    pwm_set_enabled(slice, true);
    trim = 0.0f; //reset trim
}

uint16_t duty_cycle_calc(float vin, float d_vout) {
    if (vin < 1.0f || d_vout <= 0.0f)
        return 0;  // no input or no target: off
    float dc = d_vout / vin; // ideal duty, 0.0 - 1.0
    return (uint16_t)clamp_level((int32_t)lroundf(dc * PWM_TOP));
}

//slowly ramp up duty cycle
void buck_soft_start(float vin, float d_vout) {
    int32_t target = duty_cycle_calc(vin, d_vout);
    trim = 0.0f;
    for (int32_t level = 0; level <= target; level++) {
        set_level(level);
        sleep_us(20); // ~600 steps x 20 us ~= 12 ms ramp
    }
}

//feedback loop
bool buck_regulate(float vin, float d_vout, float vout) {
    float error = d_vout - vout;            //+ means output too low

    trim += KI * error;                     // slowly build up the correction
    if (trim >  TRIM_MAX) trim =  TRIM_MAX; // anti-windup
    if (trim < -TRIM_MAX) trim = -TRIM_MAX;

    // feed-forward (ideal duty from current Vin) + feedback correction
    int32_t level = (int32_t)duty_cycle_calc(vin, d_vout) + (int32_t)lroundf(trim);
    set_level(level);

    return fabsf(error) <= VOUT_TOL; //return float abs error is less than or equal to our tolerance of 50mV
}

//disabling buck if needed
void buck_disable(void) {
    pwm_set_chan_level(slice, chan, 0);
    pwm_set_enabled(slice, false);
    trim = 0.0f;
}