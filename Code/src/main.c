#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

//spi 1 - SD CARD
const int SD_GND_D = 7;
const int SD_RX = 8;
const int SD_Csn = 9;
const int SD_SCK = 10;
const int SD_TX = 11;

//spi 0 - TFT Display
const int TFT_Csn = 17;
const int TFT_SCK = 18;
const int TFT_TX = 19;
const int TFT_RX = 20;
const int TFT_DC = 15;
const int TFT_RST = 16;


//keypad rows and cols
const int keypad_R1 = 29;
const int keypad_R2 = 30;
const int keypad_R3 = 31;
const int keypad_R4 = 32;
const int keypad_C1 = 25;
const int keypad_C2 = 26;
const int keypad_C3 = 27;
const int keypad_C4 = 28;
uint64_t keypad_mask = 0x0;
uint64_t keypad_mask = (0xFF << 25);


//I2C0 - Temp Sensor
const int TMP_INT = 35;
const int TMP_SCL = 37;
const int TMP_SDA = 36;

void keypad_isr()
{
    
}

void init_gpio()
{
    uint64_t gpio_mask = 0x0
    uint64_t gpio_mask |= (1 << 3) | (keypad_mask) ; //shift in LED bit to enable it
    gpio_init_mask64(gpio_mask); //does not initialize GPIOs for SPIs or interrupt for I2C.
    gpio_set_dir()
}

void init_spi()
{

}

void init_i2c()
{

}

void init_adc()
{

}

int main()
{
    init_gpio();
    init_spi();
    init_i2c();
    init_adc();

    for(;;)
    {

    }


    return(0);
}
