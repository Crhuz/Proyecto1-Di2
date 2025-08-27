/*
 * I2C_Weight_Slave.c
 *
 * Esclavo I2C con HX711 y control de motor DC
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include "I2C.h"
#include "UART.h"

#define SlaveAddress 0x20

// HX711
#define HX711_DT   PD2
#define HX711_SCK  PD3
#define HX711_DDR  DDRD
#define HX711_PIN  PIND
#define HX711_PORT PORTD

// Motor
#define H_DDR   DDRB
#define H_PORT  PORTB
#define IN1     PB0
#define IN2     PB1

// Variables
volatile uint8_t command = 0;
volatile long weight_value = 0;

// ---------------- HX711 ----------------
void HX711_init(void) {
    HX711_DDR &= ~(1<<HX711_DT);   // DT entrada
    HX711_DDR |= (1<<HX711_SCK);   // SCK salida
    HX711_PORT &= ~(1<<HX711_SCK);
}

long HX711_read(void) {
    unsigned long count = 0;
    while(HX711_PIN & (1<<HX711_DT)); // espera a que esté listo

    for (uint8_t i=0; i<24; i++) {
        HX711_PORT |= (1<<HX711_SCK);
        _delay_us(1);
        count = count << 1;
        HX711_PORT &= ~(1<<HX711_SCK);
        _delay_us(1);
        if (HX711_PIN & (1<<HX711_DT)) count++;
    }
    // pulso extra (ganancia 128 canal A)
    HX711_PORT |= (1<<HX711_SCK);
    _delay_us(1);
    HX711_PORT &= ~(1<<HX711_SCK);
    _delay_us(1);

    if (count & 0x800000) {
        count |= 0xFF000000; // sign extend
    }
    return (long)count;
}

// ---------------- Motor ----------------
void Motor_Stop(void) {
    H_PORT &= ~((1<<IN1)|(1<<IN2));
}
void Motor_Up(void) {
    H_PORT |= (1<<IN1);
    H_PORT &= ~(1<<IN2);
}
void Motor_Down(void) {
    H_PORT |= (1<<IN2);
    H_PORT &= ~(1<<IN1);
}
void Motor_Init(void) {
    H_DDR |= (1<<IN1) | (1<<IN2);
    Motor_Stop();
}

// ---------------- I2C ISR ----------------
ISR(TWI_vect) {
    static uint8_t byte_index = 0;

    uint8_t status = TWSR & 0xF8;
    switch (status) {
        case 0x60: // SLA+W recibido
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE)|(1<<TWEA);
            break;

        case 0x80: // dato recibido
            command = TWDR;
            if (command == 'U') Motor_Up();
            else if (command == 'D') Motor_Down();
            else if (command == 'S') Motor_Stop();
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE)|(1<<TWEA);
            break;

        case 0xA8: // SLA+R recibido
        case 0xB8: { // transmitiendo datos
            uint8_t *ptr = (uint8_t*)&weight_value;
            TWDR = ptr[byte_index];
            byte_index++;
            if (byte_index >= 4) byte_index = 0;
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE)|(1<<TWEA);
            break;
        }

        case 0xC0: // último byte transmitido, NACK recibido
        case 0xC8: // transmisión finalizada
            byte_index = 0;
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE)|(1<<TWEA);
            break;

        default:
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE)|(1<<TWEA);
            break;
    }
}

// ---------------- MAIN ----------------
int main(void) {
    UART_init();
    HX711_init();
    Motor_Init();
    I2C_Slave_Init(SlaveAddress);
    sei();

    UART_write_txt("\r\nEsclavo I2C - Peso + Motor DC listo!\r\n");

    while (1) {
        weight_value = HX711_read();   // valor crudo
        char buffer[32];
        sprintf(buffer, "Peso: %ld\r\n", weight_value);
        UART_write_txt(buffer);
		
		Motor_Up();
		_delay_ms(700);
		Motor_Stop();
		_delay_ms(800);
		Motor_Up();
		_delay_ms(700);
		Motor_Stop();
		_delay_ms(800);
		Motor_Up();
		_delay_ms(700);
		Motor_Stop();
		_delay_ms(800);
		Motor_Down();
		_delay_ms(1250);
		Motor_Stop();
		_delay_ms(3000);
		/*Motor_Down();	
		_delay_ms(1050);
		Motor_Stop();
        _delay_ms(800);*/
    }
}
