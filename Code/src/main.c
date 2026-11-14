// #include <stdio.h>
// #include <math.h>
// #include <stdlib.h>
// #include <string.h>
// #include "pico/stdlib.h"      // gpio_init, sleep_ms, stdio, etc.
// #include "hardware/gpio.h"    // full GPIO API
// #include "hardware/pwm.h"     // when you add PWM for the buck converter
// #include "hardware/adc.h"     // when you add ADC sensing



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

#include <stdio.h>
#include "pico/stdlib.h"
#include "keypad.h"


#if NUM_BANK0_GPIOS <= 32
#error "Board is configured for RP2350A (30 GPIOs); keypad C1 on GPIO 32 needs RP2350B"
#endif

int main(void) {
    stdio_init_all();
    sleep_ms(2000);   // give the USB serial port time to connect so the first prints aren't lost

    printf("\n--- Keypad test ---\n");
    printf("Press keys; each press and release should print once.\n");

    keypad_init_pins();
    keypad_init_timer();   // scanning starts ~1 s after this

    uint32_t last_beat = to_ms_since_boot(get_absolute_time());
    uint32_t presses = 0;

    for (;;) {
        if (key_available()) {
            uint16_t ev = key_pop();
            bool pressed = ev & 0x100;
            char key = (char)(ev & 0xFF);

            if (pressed) presses++;
            printf("%-8s '%c'   raw=0x%03X   total presses=%lu\n",
                   pressed ? "PRESS" : "release", key, ev, (unsigned long)presses);
        }

        // Heartbeat every 5 s so you know the program is alive even with no keys pressed
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_beat >= 5000) {
            printf("(alive, %lu presses so far)\n", (unsigned long)presses);
            last_beat = now;
        }
    }
}