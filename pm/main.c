#include <avr/interrupt.h>
#include <avr/sleep.h>


#define WAKE_UP_INTERVAL        3600

ISR(WDT_vect) {
    // do nothing
}

void sleep_1s(void) {
    WDTCR |= (0 << WDP3) | (1 << WDP2) | (1 << WDP1) | (0 << WDP0); // 1s
    // Enable watchdog timer interrupts
    WDTCR |= (1 << WDTIE);
    // Use the power down sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    // Disable brown-out detection while sleeping (20-25µA)
    sleep_bod_disable();
    // Enable global interrupts
    sei();
    // Go to sleep
    sleep_cpu();
    sleep_disable();
}


void main() {
    int cnt = 3000;      // 10 minutes before wake up

    DDRB |= _BV(PB3);    // PB3 - output
    PORTB &= ~_BV(PB3);

    PORTB |= _BV(PB0);   // PB0 - input with PU
    PORTB |= _BV(PB1);   // PB1 - input with PU
    PORTB |= _BV(PB2);   // PB2 - input with PU
    PORTB |= _BV(PB4);   // PB3 - input with PU

    while (1) {
        sleep_1s();  // Sleep for 1 seconds

        if (cnt++ >= WAKE_UP_INTERVAL) {
            // Make a pulse to switch on A9G
            PORTB |= _BV(PB3); 
            sleep_1s();
            PORTB &= ~_BV(PB3);
            cnt = 0;
        }        
    }
}
