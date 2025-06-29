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
    volatile uint8_t mode; 
    volatile uint32_t duration;
    volatile uint32_t period;
    volatile uint16_t pulse_counter;
    volatile uint8_t required_pulses;
    volatile uint8_t pending;
    volatile uint8_t running;
    volatile uint8_t in_pulse;
} pulse_config;

volatile pulse_config cfg = {
    .mode = 0,
    .duration = 100,
    .period = 1000,
    .pulse_counter = 0,
    .required_pulses = 3,
    .pending = 0,
    .running = 0,
    .in_pulse = 0,
};

volatile char serial_buffer[15];
volatile uint8_t serial_index = 0;

void setup_timer1(void) {
    // Normal port operation
    TCCR1A = 0;

    // CTC mode (Resets on OCR1A match)
    TCCR1B = (1 << WGM12); 
    
    // Enables compare match and overflow interrupts
    TIMSK1 = (1 << OCIE1A); // |  (1 << TOIE1);
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
    
    // Reset timer
    TCNT1 = 0;
    
    // Compute the number of ticks needed 
    uint32_t ticks = (F_CPU/1000)*cfg.duration/PRESCALER_VALUE - 1;

    // Set the Timer1 match value
    OCR1A = ticks > MAX_16BIT ? MAX_16BIT : ticks;

    // Restart Timer1 
    TCCR1B |=  (1 << CS12) | (1 << CS10);
    
    // Until a new interrupt occurs, we are in a positive part of the pulse
    cfg.in_pulse = 1;
}

void end_pulse(void){
    // End pulse
    PORTD &= ~(1 << OUTPUT_PIN);
    PORTB &= ~(1 << LED_PIN);

    // Stop timer
    TCCR1B &= ~((1 << CS12) | (1 << CS10));
    
    // Until a new interrupt occurs, we are outside the positive part of the pulse
    cfg.in_pulse = 0;
}

void start_offtime(void){
    // Reset Timer1
    TCNT1 = 0;
            
    // Compute the number of ticks needed
    uint32_t off_time = cfg.period - cfg.duration;
    uint32_t ticks = (F_CPU/1000)*off_time/PRESCALER_VALUE - 1;

    // Set Timer1
    OCR1A = ticks > MAX_16BIT ? MAX_16BIT : ticks;

    // Restart Timer1 
    TCCR1B |=  (1 << CS12) | (1 << CS10);

}

void end_offtime(void){
    // Stop timer
    TCCR1B &= ~((1 << CS12) | (1 << CS10));
    
    // Increment pulse counter
    cfg.pulse_counter++;
}


ISR(TIMER1_COMPA_vect){
    if (cfg.in_pulse){
        // Do stuff related to ending a pulse
        end_pulse();

        // Prepare next off time
        start_offtime();
    }else{
        // Do stuff related to ending an off time
        end_offtime();

        // Prepare next pulse
        if ((cfg.pulse_counter < cfg.required_pulses && cfg.mode == 1) || (cfg.mode == 2)){
            start_pulse();
            cfg.running = 1;
        
        // It was the last off time. End it
        }else{
            // Stop timer
            //TCCR1B &= ~((1 << CS12) | (1 << CS10));

            cfg.running = 0;
            cfg.pulse_counter = 0;
        }
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
        for (uint8_t i = 0; i<15; i++){
            serial_buffer[i] = 0;
        }
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

        // required_pulses
        case 'c':
            cfg.required_pulses = atoi(&serial_buffer[1]);
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
    
    // Signal initialization  
    PORTB |= (1 << LED_PIN);

    setup_timer1();
    setup_serial();
    sei();
    
    // Signal that initialization is done
    _delay_ms(500);
    PORTB &= ~(1 << LED_PIN);

    while (1){
        if (cfg.pending && !cfg.running){
            cfg.pending = 0;
            start_pulse();
        }
        //_delay_ms(1);
    }
}
