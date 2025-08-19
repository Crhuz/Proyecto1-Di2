/*
 * Ultrasonico_I2C_Slave.c
 *
 * Created: 18/08/2025
 * Author: Manuel
 * Description:
 *   Mide distancia con HC-SR04 y la entrega por UART
 *   y como esclavo I2C (dirección 0x30).
 *   Basado en la estructura de NFC pero reemplazado con ultrasonido.
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "UART.h"
#include "I2C.h"

// Dirección I2C del esclavo
#define SlaveAddress 0x30  

// Pines del ultrasonido
#define TRIG_PORT PORTD
#define TRIG_DDR  DDRD
#define TRIG_PIN  PD6
#define ECHO_PIN  PD7
#define ECHO_PORT PIND

volatile uint8_t buffer = 0;       // Para comunicación I2C
volatile uint8_t distancia_cm = 0; // Última distancia medida

// ------------------- UART -------------------
char uart_buf[32];

// ---------------- Ultrasonico ----------------
void Ultrasonico_Init(void) {
    TRIG_DDR |= (1<<TRIG_PIN);   // TRIG como salida
    DDRD &= ~(1<<ECHO_PIN);      // ECHO como entrada
}

uint16_t Ultrasonico_Read(void) {
    uint16_t contador = 0;

    // Pulso TRIG de 10us
    TRIG_PORT &= ~(1<<TRIG_PIN);
    _delay_us(2);
    TRIG_PORT |= (1<<TRIG_PIN);
    _delay_us(10);
    TRIG_PORT &= ~(1<<TRIG_PIN);

    // Esperar a que ECHO suba
    while (!(ECHO_PORT & (1<<ECHO_PIN)));

    // Medir ancho de pulso
    while (ECHO_PORT & (1<<ECHO_PIN)) {
        _delay_us(1);
        contador++;
    }

    // Convertir a cm
    return (contador * 0.0343) / 2;
}

// ------------------- I2C ISR -------------------
ISR(TWI_vect) {
    uint8_t estado;
    estado = TWSR & 0xFC;

    switch (estado) {
        case 0x60: // SLA+W recibido
        case 0x70:
            TWCR |= (1<<TWINT);
            break;

        case 0x80: // Datos recibidos desde Maestro
        case 0x90:
            buffer = TWDR;
            TWCR |= (1 << TWINT); 
            break;

        case 0xA8: // SLA+R recibido -> enviar dato
        case 0xB8:
            TWDR = distancia_cm;  // Enviar la última distancia medida
            TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
            break;

        default:
            TWCR |= (1 << TWINT) | (1 << TWSTO);
            break;
    }
}

// ------------------- MAIN -------------------
int main(void) {
    UART_init();
    Ultrasonico_Init();
    I2C_Slave_Init(SlaveAddress);

    sei(); // Habilitar interrupciones globales

    UART_write_txt("\r\nEsclavo I2C - Ultrasonico listo!\r\n");

    while (1) {
        uint16_t d = Ultrasonico_Read();
        if (d > 255) d = 255; // Solo un byte
        distancia_cm = (uint8_t)d;

        sprintf(uart_buf, "Distancia: %u cm\r\n", distancia_cm);
        UART_write_txt(uart_buf);

        _delay_ms(500);
    }
}
