#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "buck_SD_logger.h"

#define SYNC_INTERVAL_MS 1000   // how often data is guaranteed saved to the card

static FATFS fs;
static FIL   file;
static bool  log_open = false;
static uint32_t last_sync_ms;

bool sd_log_init(void) {
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("SD: mount failed (FRESULT %d)\n", fr);
        return false;
    }

    // Find the first unused name: LOG000.CSV, LOG001.CSV, ...
    char name[16];
    for (int n = 0; n < 1000; n++) {
        snprintf(name, sizeof name, "LOG%03d.CSV", n);
        fr = f_open(&file, name, FA_WRITE | FA_CREATE_NEW);
        if (fr == FR_OK) break;
        if (fr != FR_EXIST) {
            printf("SD: open failed (FRESULT %d)\n", fr);
            return false;
        }
    }
    if (fr != FR_OK) {
        printf("SD: no free file names\n");
        return false;
    }

    const char *header = "time_ms,vin_V,iin_A,vout_V,iout_A,pin_W,pout_W\r\n";
    UINT bw;
    f_write(&file, header, (UINT)strlen(header), &bw);
    f_sync(&file);

    log_open = true;
    last_sync_ms = to_ms_since_boot(get_absolute_time());
    printf("SD: logging to %s\n", name);
    return true;
}

bool sd_log_write(const adc_readings_t *r) {
    if (!log_open) return false;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    char line[96];
    int len = snprintf(line, sizeof line,
                       "%lu,%.3f,%.4f,%.3f,%.4f,%.3f,%.3f\r\n",
                       (unsigned long)now,
                       r->vin, r->iin, r->vout, r->iout,
                       r->vin * r->iin, r->vout * r->iout);

    UINT bw;
    if (f_write(&file, line, (UINT)len, &bw) != FR_OK || bw != (UINT)len)
        return false;

    // f_write buffers in RAM; f_sync pushes it to the card so a power loss
    // or card removal loses at most ~1 s of data
    if (now - last_sync_ms >= SYNC_INTERVAL_MS) {
        f_sync(&file);
        last_sync_ms = now;
    }
    return true;
}

void sd_log_close(void) {
    if (log_open) {
        f_close(&file);
        log_open = false;
    }
    f_unmount("");
}
