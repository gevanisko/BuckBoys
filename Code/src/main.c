#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"      // gpio_init, sleep_ms, stdio, etc.
#include "hardware/gpio.h"    // full GPIO API
#include "hardware/pwm.h"     // when you add PWM for the buck converter
#include "hardware/adc.h"
#include "buck_adc.h"    // when you add ADC sensing
#include "buck_pwm.h"
#include "buck_adc.h"
#include "buck_SD_logger.h"


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



#define LOG_PERIOD_MS   100     // 10 rows per second
#define LOG_DURATION_MS 30000   // 30 s test

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("\n--- SD card logging test ---\n");

    init_adc();
    if (!sd_log_init()) {
        printf("Check: card inserted? FAT32 formatted? wiring?\n");
        while (true) tight_loop_contents();
    }

    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint32_t next  = start;
    int rows = 0;

    while (to_ms_since_boot(get_absolute_time()) - start < LOG_DURATION_MS) {
        adc_readings_t r;
        adc_start_capture();
        while (!adc_capture_done()) { }
        adc_buf_average(&r);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((int32_t)(now - next) >= 0) {
            if (!sd_log_write(&r)) printf("SD: write failed\n");
            rows++;
            next += LOG_PERIOD_MS;
            if (rows % 10 == 0)
                printf("%d rows  (Vin %.2f V  Vout %.2f V)\n", rows, r.vin, r.vout);
        }
    }

    sd_log_close();
    printf("Done: %d rows written. Safe to remove the card.\n", rows);
    while (true) tight_loop_contents();
}