#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#define IIN  42
#define VIN  43
#define IOUT 44
#define VOUT 45
#define ADC_CH(pin) ((pin) - 40)     // RP2350B: GPIO 40-47 = ADC ch 0-7

#define CH_NUM 4
#define SETS 512  // 512 sets x 21.25 us = 10.88 ms = 1088 PWM periods
#define BUF_LEN (CH_NUM * SETS)

#define VIN_DIV_RATIO (33.0f / 133.0f)   
#define VOUT_DIV_RATIO (33.0f / 55.0f)
#define IIN_V_PER_A 2.0f
#define IOUT_V_PER_A 2.0f

#define ADC_VOLTS_PER_COUNT (3.3f / 4096.0f)

static uint16_t adc_buf[BUF_LEN];    // IIN, VIN, IOUT, VOUT, IIN, VIN, ...
static int dma_chan;

typedef struct {
    float iin; // A
    float vin; // V
    float iout; // A
    float vout; // V
} adc_readings_t;

void init_adc(void)
{
    adc_init();
    adc_gpio_init(IIN);
    adc_gpio_init(VIN);
    adc_gpio_init(IOUT);
    adc_gpio_init(VOUT);

    adc_set_round_robin(0x3C); // channels 2-5
    adc_fifo_setup(true, true, 1, false, false);           
    adc_set_clkdiv(254.0f); // 188.2 kS/s total -> each channel every 21.25 us

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false); // always read the FIFO register
    channel_config_set_write_increment(&c, true); // move through adc_buf
    channel_config_set_dreq(&c, DREQ_ADC);        // copy only when a sample is ready

    dma_channel_configure(dma_chan, &c, adc_buf, &adc_hw->fifo, BUF_LEN, false);
}

// Start one capture. Returns right away; the hardware fills adc_buf.
void adc_start_capture(void)
{
    adc_run(false);
    while (!(adc_hw->cs & ADC_CS_READY_BITS)); // let any conversion finish
    adc_fifo_drain(); // throw away leftover samples
    adc_select_input(ADC_CH(IIN));// order starts at IIN again

    dma_channel_set_write_addr(dma_chan, adc_buf, true); //buffer start + START the DMA
    adc_run(true);
}

bool adc_capture_done(void)
{
    if (dma_channel_is_busy(dma_chan))
        return false;
    adc_run(false); // buffer full: stop converting
    return true;
}

//Average each channel over the whole buffer (call after the capture is done)
void adc_buf_average(adc_readings_t *r)
{
    uint32_t sum[CH_NUM] = {0};
    for (int i = 0; i < BUF_LEN; i++)
        sum[i % CH_NUM] += adc_buf[i];

    float pin[CH_NUM];   // average voltage at each ADC pin
    for (int ch = 0; ch < CH_NUM; ch++)
        pin[ch] = ((float)sum[ch] / SETS) * ADC_VOLTS_PER_COUNT;

    r->iin  = pin[0] / IIN_V_PER_A;     // amps
    r->vin  = pin[1] / VIN_DIV_RATIO;   // volts
    r->iout = pin[2] / IOUT_V_PER_A;    // amps
    r->vout = pin[3] / VOUT_DIV_RATIO;  // volts
}
