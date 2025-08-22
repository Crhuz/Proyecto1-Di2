/*
 * I2C_Weight_Slave.c
 *
 * Created: 21/08/2025
 * Author: Manuel
 * Description:
 *   Esclavo I2C que mide peso con HX711 y envía datos al maestro
 *   Además controla un motor DC mediante puente H con comandos del maestro:
 *      'U' -> subir (Up)
 *      'D' -> bajar (Down)
 *      'S' -> stop (frenar)
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include "I2C.h"
#include "UART.h"

// Dirección I2C del esclavo
#define SlaveAddress 0x20

// Pines HX711
#define HX711_DT   PD2
#define HX711_SCK  PD3
#define HX711_DDR  DDRD
#define HX711_PIN  PIND
#define HX711_PORT PORTD

// Motor Puente H
#define H_DDR   DDRB
#define H_PORT  PORTB
#define IN1     PB0
#define IN2     PB1

// Variables
volatile uint8_t command = 0;
volatile long weight_value = 0;
void Motor_Stop(void);

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
    // pulso extra para canal A, ganancia 128
    HX711_PORT |= (1<<HX711_SCK);
    _delay_us(1);
    HX711_PORT &= ~(1<<HX711_SCK);
    _delay_us(1);

    // convertir a signed 24 bits
    if (count & 0x800000) {
        count |= 0xFF000000;
    }
    return (long)count;
}

// ---------------- Motor ----------------
void Motor_Init(void) {
    H_DDR |= (1<<IN1) | (1<<IN2);
    Motor_Stop();
}

void Motor_Up(void) {
    H_PORT |= (1<<IN1);
    H_PORT &= ~(1<<IN2);
}

void Motor_Down(void) {
    H_PORT |= (1<<IN2);
    H_PORT &= ~(1<<IN1);
}

void Motor_Stop(void) {
    H_PORT &= ~((1<<IN1)|(1<<IN2));
}


ISR(TWI_vect) {
	uint8_t status = TWSR & 0xF8;

	switch (status) {
		case 0x60: // SLA+W recibido
		TWCR |= (1<<TWINT);
		break;

		case 0x80: // Dato recibido
		command = TWDR;
		if (command == 'U') Motor_Up();
		else if (command == 'D') Motor_Down();
		else if (command == 'S') Motor_Stop();
		TWCR |= (1<<TWINT);
		break;

		case 0xA8: // SLA+R recibido (maestro pide datos)
		case 0xB8: {
			static uint8_t i = 0;
			if (i == 0) {
				TWDR = (weight_value >> 8) & 0xFF; // parte alta
				i++;
				} else {
				TWDR = weight_value & 0xFF; // parte baja
				i = 0;
			}
			TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA);
			break;
		}

		default:
		TWCR |= (1<<TWINT)|(1<<TWSTO);
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
        weight_value = HX711_read();   // obtiene valor crudo
        char buffer[20];
        sprintf(buffer, "Peso: %ld\r\n", weight_value);
        UART_write_txt(buffer);
        _delay_ms(500);
    }
}
