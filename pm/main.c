#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>


// Pin 4 for LED
int led = 4;
ISR(WDT_vect) {
    //digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
//    delay(1000);               // wait for a second
    //digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
//    sleep_mode();
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
    DDRB |= _BV( PB3 );

    while( 1 ) {
        PORTB |=  _BV( PB3 );
        idle();
        PORTB &=~ _BV( PB3 );
        idle();
    }
}