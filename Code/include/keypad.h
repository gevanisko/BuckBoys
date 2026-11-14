#ifndef KEYPAD_H
#define KEYPAD_H

#include "queue.h"

// Configure keypad GPIOs (columns = outputs, rows = inputs with pull-downs)
void keypad_init_pins(void);

// Start the timer-driven scan (TIMER0 ALARM0 = drive column, ALARM1 = read rows).
// Key events then appear in the queue: use key_available() / key_pop().
void keypad_init_timer(void);

#endif