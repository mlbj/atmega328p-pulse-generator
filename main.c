#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define LED_PIN     PB5
#define OUTPUT_PIN  PD3
#define PRESCALER_VALUE 1024
#define MAX_16BIT   65535


//volatile uint8_t pulse_requested = 0;
typedef struct{
    uint8_t mode; 
    uint16_t duration;
    uint16_t period;
    uint8_t count;
    uint8_t pending;
    uint8_t running;
} pulse_config;

volatile pulse_config cfg = {
    .mode = 0,
    .duration = 100,
    .period = 1000,
    .count = 1,
    .pending = 0,
    .running = 0,
};

volatile uint8_t pulse_counter = 0;
volatile char serial_buffer[15];
volatile uint8_t serial_index = 0;

void setup_timer1(void) {
    // Normal port operation
    TCCR1A = 0;

    // CTC mode (Resets on OCR1A match)
    TCCR1B = (1 << WGM12); 
    
    // Enables compare match interrupt
    TIMSK1 = (1 << OCIE1A);
}

void setup_serial(){
    // 9600 baud rate at 16 MHz
    UBRR0 = 103; 

    UCSR0B = (1 << RXEN0) | (1 << RXCIE0);
    //UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data
}

void start_pulse(){
    PORTD |= (1 << OUTPUT_PIN);
    PORTB |= (1 << LED_PIN);
    
    cfg.running = 1;

    // Reset timer
    TCNT1 = 0;
    
    uint32_t ticks = (F_CPU/1000)*cfg.duration/PRESCALER_VALUE - 1;
    OCR1A = ticks > MAX_16BIT ? MAX_16BIT : ticks;

    // Selects Timer1 prescaler 
    TCCR1B |=  (1 << CS12) | (1 << CS10); 
}

ISR(TIMER1_COMPA_vect){
    // This tracks if we are about to end a pulse or enter a new pulse
    // in multi mode. If in_pulse == 1, then we are at the end of a pulse
    static uint8_t in_pulse = 1;
    
    if (in_pulse){
        // End pulse
        PORTD &= ~(1 << OUTPUT_PIN);
        PORTB &= ~(1 << LED_PIN);
        
        
        // Continuous mode
        if (cfg.mode == 2){
            static uint16_t cycle_count = 0;
            if (++cycle_count >= (cfg.period/cfg.duration)){
                cycle_count = 0;
                start_pulse();
            }
        // Multi mode
        }else if(cfg.mode == 1){
            // At least one more pulse in Multi mode
            if (++pulse_counter < cfg.count){
                // Calculate OFF time
                uint16_t off_time = cfg.period - cfg.duration;

                // Schedule next pulse after period
                uint32_t ticks = (F_CPU/1000)*off_time/PRESCALER_VALUE - 1;
                OCR1A = ticks > MAX_16BIT ? MAX_16BIT : ticks;
                TCNT1 = 0;
            
                // Restart timer
                TCCR1B |= (1 << CS12) | (1 << CS10);
                
                // Set in_pulse to 0, so that the next ISR will start a new pulse
                in_pulse = 0;
            // End of last pulse in Multi mode
            }else{
                cfg.running = 0;
                pulse_counter = 0;

                // Stop timer
                TCCR1B &= ~((1 << CS12) | (1 << CS10)); 
            }
        // Single mode
        }else{
            cfg.running = 0;
        
            // Stop timer
            TCCR1B &= ~((1 << CS12) | (1 << CS10)); 
        }
    // Start a new pulse in Multi mode
    }else{
        start_pulse();
        in_pulse = 1;
    }      
}

ISR(USART_RX_vect){
    /*// Prevent buffer overflow
    if (serial_index >= 15){
        serial_index = 0;
        return;
    }*/

    char data = UDR0;

    // Process on end of command packet
    if (data == '\n'){
        process_command();
        serial_index = 0;
    // Fill the command buffer
    }else if(serial_index < 15){
        serial_buffer[serial_index++] = data;
    }
}

void process_command(){
    char cmd = serial_buffer[0];

    switch(cmd){
        // mode (0=Single, 1=Multi, 2=Continuous
        case 'm':
            cfg.mode = serial_buffer[1] - '0';
            break;

        // duration (in ms)
        case 'd':
            cfg.duration = atoi(&serial_buffer[1]);
            break;

        // period (in ms)
        case 'p':
            cfg.period = atoi(&serial_buffer[1]);

            // This is not optimal, since one can decide to change the period first
            // and then change the duration
            //if(cfg.period < cfg.duration){
            //    cfg.period = cfg.duration;
            //}
            break;

        // count
        case 'c':
            cfg.count = atoi(&serial_buffer[1]);
            break;

        // trigger
        case 't':
            if (!cfg.running){
                cfg.pending = 1;
            }
            break;
        
        // stop
        case 's':
            cfg.running = 0;
            
            // Stop timer
            TCCR1B &= ~((1 << CS12) | (1 << CS10)); 
            
            // Turn off output and LED pins
            PORTD &= ~(1 << OUTPUT_PIN);
            PORTB &= ~(1 << LED_PIN);
            break;
    
        // invalid command
        default:
            for(uint8_t i=0; i<20; i++) {
                PORTB ^= (1 << LED_PIN);
                _delay_ms(40);
            }
            break;
    }
}

int main() {
    // Configure output and LED pins as Output
    DDRB |= (1 << LED_PIN);
    DDRD |= (1 << OUTPUT_PIN);
    
    setup_timer1();
    setup_serial();
    sei();
    
    // Signal that initialization is done
    PORTB |= (1 << LED_PIN);
    _delay_ms(1000);
    PORTB &= ~(1 << LED_PIN);

    while (1){
        if (cfg.pending && !cfg.running){
            cfg.pending = 0;
            start_pulse();
        }
        //_delay_ms(1);
    }
}
