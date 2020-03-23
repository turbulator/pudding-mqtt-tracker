#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>


#define WAKE_UP_INTERVAL        450
#define BATTERY_LOW_THRESHOLD   744  // ~3,2V

ISR(WDT_vect) {
    // do nothing
}

void sleep(uint8_t prescaler) {
    // Disable ADC
    ADCSRA &= ~(1 << ADEN);
    // Set WDT prescaler
    WDTCR |= prescaler;
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

uint16_t read_adc(void)
{
    // Enable ADC with prescaler = 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADMUX = (1 << REFS0) | (1 << MUX0); // Set internal voltage reference and ADC1
    ADCSRA |= (1 << ADSC);              // Start conversion

    while(ADCSRA & (1 << ADSC)) {
        // Wait for conversion complete
    }   

    return ADC;
}

void main() {
    int cnt = WAKE_UP_INTERVAL - 75;  // 10 minutes before wake up
    int voltage;

    DDRB |= _BV(PB0);    // PB0 - A9G enable
    DDRB |= _BV(PB1);    // PB1 - Battery voltage divider enable
    // Switch off all digital inputs to reduce the power consumption
    DIDR0 |= (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D) | (1 << ADC3D); 

    while (1) {
        sleep(WDTO_8S);  // Sleep for 8 seconds

        if (cnt++ >= WAKE_UP_INTERVAL) {

            // Check the battery
            PORTB |= _BV(PB1);
            sleep(WDTO_30MS);
            voltage = read_adc();
            PORTB &= ~_BV(PB1);

            if (voltage > BATTERY_LOW_THRESHOLD) {
                // Make a pulse to switch on A9G
                PORTB |= _BV(PB0);
                sleep(WDTO_4S);
                PORTB &= ~_BV(PB0);
            }
            cnt = 0;
        }
    }
}
