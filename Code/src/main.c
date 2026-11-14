#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"      // gpio_init, sleep_ms, stdio, etc.
#include "hardware/gpio.h"    // full GPIO API
#include "hardware/pwm.h"     // when you add PWM for the buck converter
#include "hardware/adc.h"
#include "adc.h"    // when you add ADC sensing



// //spi 1 - SD CARD
// const int SD_GND_D = 7;
// const int SD_RX = 8;
// const int SD_Csn = 9;
// const int SD_SCK = 10;
// const int SD_TX = 11;

// //spi 0 - TFT Display
// const int TFT_Csn = 17;
// const int TFT_SCK = 18;
// const int TFT_TX = 19;
// const int TFT_RX = 20;
// const int TFT_DC = 15;
// const int TFT_RST = 16;



// //I2C0 - Temp Sensor
// const int TMP_INT = 35;
// const int TMP_SCL = 37;
// const int TMP_SDA = 36;

// void init_spi()
// {

// }

// void init_i2c()
// {

// }

// void init_adc()
// {

// }

// int main()
// {
//     stdio_init_all();
//     init_gpio();
//     keypad_init_timer();
//     init_spi();
//     init_i2c();
//     init_adc();

//     for(;;)
//     {

//     }


//     return(0);
// }
int main(void) {
    stdio_init_all();
    sleep_ms(2000);   // let USB serial connect

    printf("\n--- ADC DMA test (scaled) ---\n");
    init_adc();

    while (true) {
        adc_start_capture();
        while (!adc_capture_done()) {
            // ~11 ms capture; CPU is free here
        }

        adc_readings_t r;
        adc_buf_average(&r);

        float pin  = r.vin  * r.iin;
        float pout = r.vout * r.iout;
        float eff  = (pin > 0.01f) ? 100.0f * pout / pin : 0.0f;

        printf("IN:  %6.2f V  %6.3f A  %6.2f W\n", r.vin,  r.iin,  pin);
        printf("OUT: %6.2f V  %6.3f A  %6.2f W   eff %5.1f%%\n\n",
               r.vout, r.iout, pout, eff);

        sleep_ms(500);
    }
}