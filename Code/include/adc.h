#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

// Raw 12-bit readings (0-4095), one per channel
typedef struct {
    float iin;    // A
    float vin;    // V
    float iout;   // A
    float vout;   // V
} adc_readings_t;

void  init_adc(void);                       // set up ADC + DMA (call once)
void  adc_start_capture(void);              // start filling the buffer (returns right away)
bool  adc_capture_done(void);               // true once the buffer is full
void  adc_buf_average(adc_readings_t *r);   // average each channel over the buffer
float adc_to_output(uint16_t raw);          // raw -> volts at the ADC pin

#endif