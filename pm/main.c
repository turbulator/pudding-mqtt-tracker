#include <avr/interrupt.h>
#include <avr/sleep.h>


#define ALARM_INTERVAL  450      // 450 * 8 = 3600 seconds

ISR(WDT_vect) {
    // do nothing
}

void idle(void) {
    WDTCR |= (1 << WDP3) | (0 << WDP2) | (0 << WDP1) | (1 << WDP0); // 8s
    // Enable watchdog timer interrupts
    WDTCR |= (1 << WDTIE);
    // Use the Power Down sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    // Disable brown-out detection while sleeping (20-25µA)
    sleep_bod_disable();
    sei(); // Enable global interrupts
    sleep_cpu();                   //go to sleep
    sleep_disable();
}


void main() {
    int alarm = 0;       
    
    DDRB |= _BV(PB3);    // PB3 - output
    PORTB &= ~_BV(PB3);

    PORTB |= _BV(PB0);   // PB0 - input with PU
    PORTB |= _BV(PB1);   // PB1 - input with PU
    PORTB |= _BV(PB2);   // PB2 - input with PU
    PORTB |= _BV(PB4);   // PB3 - input with PU

    while (1) {
        idle();  // Sleep for 8 seconds

        if (PINB & _BV(PB2)) {   // IGN is off
            alarm++;
        } else {                // IGN is on
            alarm = 0;
        }

        // Switch on A9G
        if (alarm >= ALARM_INTERVAL) {
            PORTB |= _BV(PB3); 
            idle();
            PORTB &= ~_BV(PB3);
            alarm = 0;
        }        
    }
}
