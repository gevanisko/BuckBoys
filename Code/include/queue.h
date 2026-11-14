#ifndef QUEUE_H
#define QUEUE_H

#include <hardware/timer.h>
#include "pico/stdlib.h"

typedef struct {
    volatile uint16_t q[32];
    volatile uint16_t head;
    volatile uint16_t tail;
} KeyEvents;

extern KeyEvents kev;   // defined in keypad.c

// true if an event is waiting (never blocks)
static inline bool key_available(void) {
    return kev.head != kev.tail;
}

// waits until an event is available, then returns it
static inline uint16_t key_pop(void) {
    while (kev.head == kev.tail) {
        sleep_ms(10);
    }
    uint16_t value = kev.q[kev.tail];
    kev.tail = (kev.tail + 1) % 32;
    return value;
}

// called from the keypad ISR; drops the key if the queue is full
static inline void key_push(uint16_t value) {
    if ((kev.head + 1) % 32 == kev.tail) {
        return;
    }
    kev.q[kev.head] = value;
    kev.head = (kev.head + 1) % 32;
}

#endif