#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include <math.h>

#define PWM_8A 40
#define SLICENUM 8

uint16_t duty_cycle_calc(float vin, float d_vout)
{
    float dc = (d_vout / vin);
    uint16_t CC = (uint16_t) (dc * 100000);
    return CC;
}

void pwm_init(uint32_t dc, float vin, float d_vout)
{
    gpio_set_function(40, GPIO_FUNC_PWM);

    pwm_set_clkdiv(SLICENUM, 1500);

    uint16_t level = duty_cycle_calc(vin, d_vout);

    pwm_set_chan_level(SLICENUM, 0, level);
    pwm_set_wrap(SLICENUM, 99999);
    pwm_set_enabled(SLICENUM, true);
}



bool pwm_feedback(float vin, float d_vout, float prev_v, float curr_v)
{
    if(((curr_v - d_vout) <= 0.1) || (d_vout - curr_v) <= 0.1)
        return true;
    else if (curr_v - d_vout < 0) {
        pwm_set_chan_level(SLICENUM, 0,  (pwm_hw->slice[SLICENUM].cc + 21));
        return false;
    } else if (curr_v - d_vout > 0) {
        pwm_set_chan_level(SLICENUM, 0,  (pwm_hw->slice[SLICENUM].cc - 21));
        return false; 
    }
}