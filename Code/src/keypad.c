
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"      // gpio_init, sleep_ms, stdio, etc.
#include "hardware/gpio.h"    // full GPIO API
#include "hardware/pwm.h"     // when you add PWM for the buck converter
#include "hardware/adc.h" 
#include "queue.h"

#define KEYPAD_C1 32   // GPIO 32 is above 31, so no 32-bit masks are used below
#define KEYPAD_C2 31
#define KEYPAD_C3 30
#define KEYPAD_C4 29
#define KEYPAD_R1 28
#define KEYPAD_R2 27
#define KEYPAD_R3 26
#define KEYPAD_R4 25

// Index 0 = C1/R1 ... index 3 = C4/R4, so pin order doesn't matter.
static const uint col_pins[4] = { KEYPAD_C1, KEYPAD_C2, KEYPAD_C3, KEYPAD_C4 };
static const uint row_pins[4] = { KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4 };

#define SCAN_PERIOD_US 25000u

// The one definition of the global queue declared in queue.h
KeyEvents kev;

// keymap[col * 4 + row], assuming standard keypad labels:
// C1 = left column (1 4 7 *), R1 = top row (1 2 3 A).
//        R1   R2   R3   R4
//  C1:  '1'  '4'  '7'  '*'
//  C2:  '2'  '5'  '8'  '0'
//  C3:  '3'  '6'  '9'  '#'
//  C4:  'A'  'B'  'C'  'D'
static const char keymap[17] = "147*2580369#ABCD";

static volatile int col = -1;
static bool state[16];   // true = key currently held

// ---------------- GPIO ----------------
void keypad_init_pins(void) {
    for (int i = 0; i < 4; i++) {
        // Columns: outputs, start low
        gpio_init(col_pins[i]);
        gpio_set_dir(col_pins[i], GPIO_OUT);
        gpio_put(col_pins[i], 0);

        // Rows: inputs, idle LOW; read high only when a pressed key connects
        // them to the driven column.
        // External pull-downs (~8.2k or less) recommended due to RP2350 erratum E9.
        gpio_init(row_pins[i]);
        gpio_set_dir(row_pins[i], GPIO_IN);
        gpio_pull_down(row_pins[i]);
    }
}

// Returns rows as bits 0-3 (bit 0 = R1 ... bit 3 = R4).
static uint8_t keypad_read_rows(void) {
    uint8_t rows = 0;
    for (int r = 0; r < 4; r++) {
        if (gpio_get(row_pins[r]))
            rows |= (uint8_t)(1u << r);
    }
    return rows;
}

// ---------------- Timer ISRs ----------------
// ALARM0: drive the next column high, all others low.
static void keypad_drive_column(void) {
    timer0_hw->intr = 1u << 0;   // acknowledge ALARM0 (write 1 to clear)

    col = (col + 1) & 3;         // -1 -> 0, then 0,1,2,3,0,...
    for (int c = 0; c < 4; c++)
        gpio_put(col_pins[c], c == col);

    timer0_hw->alarm[0] = timer0_hw->timerawl + SCAN_PERIOD_US;
}

// ALARM1 (10 ms after ALARM0): read rows for the driven column, queue changes.
static void keypad_isr(void) {
    timer0_hw->intr = 1u << 1;   // acknowledge ALARM1

    int c = col;
    if (c >= 0) {
        uint8_t rows = keypad_read_rows();
        for (uint8_t row = 0; row < 4; row++) {
            uint32_t ind = (uint32_t)c * 4 + row;
            bool is_high = (rows >> row) & 1u;

            if (is_high && !state[ind]) {          // newly pressed
                state[ind] = true;
                key_push((uint16_t)(0x100 | (uint8_t)keymap[ind]));
            } else if (!is_high && state[ind]) {   // newly released
                state[ind] = false;
                key_push((uint16_t)(0x000 | (uint8_t)keymap[ind]));
            }
        }
    }

    timer0_hw->alarm[1] = timer0_hw->timerawl + SCAN_PERIOD_US;
}

void keypad_init_timer(void) {
    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);

    hw_set_bits(&timer0_hw->inte, (1u << 0) | (1u << 1));

    // SDK equivalent of the lab's NVIC ICPR/ISER writes
    irq_set_enabled(TIMER0_IRQ_0, true);
    irq_set_enabled(TIMER0_IRQ_1, true);

    uint32_t now = timer0_hw->timerawl;
    timer0_hw->alarm[0] = now + 1000000u;   // first column drive at +1.00 s
    timer0_hw->alarm[1] = now + 1010000u;   // first read at +1.01 s
}