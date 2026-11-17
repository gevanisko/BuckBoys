// SD card over SPI: the "disk I/O" layer that FatFs calls to read/write
// 512-byte sectors. Replaces the template diskio.c that ships with FatFs.

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "ff.h"
#include "diskio.h"

// ---------------- Pins (SPI1) ----------------
#define SD_SPI       spi1
#define SD_MISO_PIN  8     // SPI1 RX  <- card DO
#define SD_CS_PIN    9     // driven as a plain GPIO, not hardware CSn
#define SD_SCK_PIN   10    // SPI1 SCK
#define SD_MOSI_PIN  11    // SPI1 TX  -> card DI

#define SD_INIT_BAUD 400000     // cards must be initialized at <= 400 kHz
#define SD_FAST_BAUD 12500000   // after init; SD allows up to 25 MHz

// ---------------- SD commands ----------------
#define CMD0    0              // GO_IDLE_STATE (reset)
#define CMD8    8              // SEND_IF_COND (check voltage, SD v2)
#define CMD16   16             // SET_BLOCKLEN
#define CMD17   17             // READ_SINGLE_BLOCK
#define CMD24   24             // WRITE_BLOCK
#define CMD55   55             // APP_CMD (prefix for ACMDs)
#define CMD58   58             // READ_OCR
#define ACMD41  (0x80 | 41)    // SD_SEND_OP_COND (start init)

static volatile DSTATUS stat = STA_NOINIT;
static bool block_addr = false;   // true for SDHC/SDXC (address by block, not byte)

// ---------------- Low-level SPI helpers ----------------
static uint8_t xchg(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(SD_SPI, &b, &r, 1);
    return r;
}

// Card holds DO low while busy; wait until it sends 0xFF
static bool wait_ready(uint32_t timeout_ms) {
    absolute_time_t t = make_timeout_time_ms(timeout_ms);
    do {
        if (xchg(0xFF) == 0xFF) return true;
    } while (!time_reached(t));
    return false;
}

static void sd_deselect(void) {
    gpio_put(SD_CS_PIN, 1);
    xchg(0xFF);                // extra clocks so the card releases DO
}

static bool sd_select(void) {
    gpio_put(SD_CS_PIN, 0);
    xchg(0xFF);
    if (wait_ready(500)) return true;
    sd_deselect();
    return false;
}

// Send a command frame, return the 1-byte R1 response (0 = OK, 0x01 = idle)
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    if (cmd & 0x80) {                      // ACMD: send CMD55 first
        cmd &= 0x7F;
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1) return r;
    }

    sd_deselect();
    if (!sd_select()) return 0xFF;

    uint8_t frame[6] = {
        (uint8_t)(0x40 | cmd),
        (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
        (uint8_t)(arg >> 8),  (uint8_t)arg,
        0x01                               // CRC only checked for CMD0/CMD8
    };
    if (cmd == CMD0) frame[5] = 0x95;
    if (cmd == CMD8) frame[5] = 0x87;
    spi_write_blocking(SD_SPI, frame, sizeof frame);

    uint8_t r = 0xFF;
    for (int i = 0; i < 10; i++) {         // response arrives within 8 bytes
        r = xchg(0xFF);
        if (!(r & 0x80)) break;
    }
    return r;
}

// Receive one data block (after CMD17)
static bool rcvr_block(uint8_t *buf, uint len) {
    absolute_time_t t = make_timeout_time_ms(200);
    uint8_t token;
    do {
        token = xchg(0xFF);
    } while (token == 0xFF && !time_reached(t));
    if (token != 0xFE) return false;       // 0xFE = data start token

    spi_read_blocking(SD_SPI, 0xFF, buf, len);
    xchg(0xFF);                            // CRC (ignored)
    xchg(0xFF);
    return true;
}

// Send one 512-byte data block (after CMD24)
static bool xmit_block(const uint8_t *buf) {
    if (!wait_ready(500)) return false;
    xchg(0xFE);                            // data start token
    spi_write_blocking(SD_SPI, buf, 512);
    xchg(0xFF);                            // dummy CRC
    xchg(0xFF);
    uint8_t resp = xchg(0xFF);
    if ((resp & 0x1F) != 0x05) return false;   // 0x05 = data accepted
    return wait_ready(500);                // card is busy writing to flash
}

// ---------------- FatFs disk I/O interface ----------------
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;

    spi_init(SD_SPI, SD_INIT_BAUD);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(SD_MISO_PIN);             // card DO is open-drain-ish at startup

    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1);

    sleep_ms(10);                          // let card power settle
    for (int i = 0; i < 10; i++) xchg(0xFF);   // >= 74 clocks with CS high

    stat = STA_NOINIT;
    block_addr = false;

    if (send_cmd(CMD0, 0) == 1) {                      // card is in idle state
        absolute_time_t t = make_timeout_time_ms(1000);

        if (send_cmd(CMD8, 0x1AA) == 1) {              // SD v2 card
            uint8_t r7[4];
            spi_read_blocking(SD_SPI, 0xFF, r7, 4);
            if (r7[2] == 0x01 && r7[3] == 0xAA) {      // accepts 2.7-3.6 V
                while (!time_reached(t) && send_cmd(ACMD41, 1UL << 30) != 0) { }
                if (!time_reached(t) && send_cmd(CMD58, 0) == 0) {
                    uint8_t ocr[4];
                    spi_read_blocking(SD_SPI, 0xFF, ocr, 4);
                    block_addr = (ocr[0] & 0x40) != 0; // CCS bit: SDHC/SDXC
                    if (block_addr || send_cmd(CMD16, 512) == 0)
                        stat = 0;
                }
            }
        } else {                                       // old SD v1 card
            while (!time_reached(t) && send_cmd(ACMD41, 0) != 0) { }
            if (!time_reached(t) && send_cmd(CMD16, 512) == 0)
                stat = 0;
        }
    }
    sd_deselect();

    if (stat == 0)
        spi_set_baudrate(SD_SPI, SD_FAST_BAUD);
    return stat;
}

DSTATUS disk_status(BYTE pdrv) {
    return (pdrv == 0) ? stat : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (stat & STA_NOINIT) return RES_NOTRDY;

    uint32_t addr = (uint32_t)sector;
    if (!block_addr) addr *= 512;          // SDSC cards use byte addresses

    for (; count; count--) {
        if (send_cmd(CMD17, addr) != 0 || !rcvr_block(buff, 512)) {
            sd_deselect();
            return RES_ERROR;
        }
        buff += 512;
        addr += block_addr ? 1 : 512;
    }
    sd_deselect();
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (stat & STA_NOINIT) return RES_NOTRDY;

    uint32_t addr = (uint32_t)sector;
    if (!block_addr) addr *= 512;

    for (; count; count--) {
        if (send_cmd(CMD24, addr) != 0 || !xmit_block(buff)) {
            sd_deselect();
            return RES_ERROR;
        }
        buff += 512;
        addr += block_addr ? 1 : 512;
    }
    sd_deselect();
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    (void)buff;
    if (pdrv != 0) return RES_PARERR;
    if (stat & STA_NOINIT) return RES_NOTRDY;

    if (cmd == CTRL_SYNC) {                // make sure the last write finished
        bool ok = sd_select();
        sd_deselect();
        return ok ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;                     // other commands only needed for formatting
}

// FatFs needs a timestamp for files. No real-time clock, so use a fixed date.
DWORD get_fattime(void) {
    return ((DWORD)(2026 - 1980) << 25)    // year
         | ((DWORD)1 << 21)                // month
         | ((DWORD)1 << 16);               // day, 00:00:00
}