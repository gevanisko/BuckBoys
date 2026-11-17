#ifndef BUCK_SD_LOGGER_H
#define BUCK_SD_LOGGER_H
 
#include <stdbool.h>
#include "buck_adc.h"
 
// Mount the SD card, create the next free LOGnnn.CSV, and write the header row.
bool sd_log_init(void);
 
// Append one row: time_ms, vin, iin, vout, iout, pin, pout.
// Saves to the card (f_sync) about once a second.
bool sd_log_write(const adc_readings_t *r);
 
// Finish the file. Call before removing the card to keep every row.
void sd_log_close(void);
 
#endif