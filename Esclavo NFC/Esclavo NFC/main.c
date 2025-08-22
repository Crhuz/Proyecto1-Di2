/*
 * NFC_I2C_Slave.c
 *
 * Created: 21/08/2025
 * Author: Manuel
 * Description:
 *   Esclavo I2C (0x30) con:
 *      - Lector RFID RC522 (NFC)
 *      - Control de talanquera con servo
 *   Funcionalidad:
 *      - Detecta tarjeta y abre talanquera automáticamente
 *      - Envía UID completo (5 bytes) al maestro cuando este lo solicite con 'R'
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include "UART.h"
#include "ALB_RFID.h"
#include "I2C.h"

// Dirección I2C del esclavo
#define SlaveAddress 0x30  

// ---------------- Variables ----------------
volatile uint8_t buffer = 0;        // Último comando recibido por I2C
uint8_t uid[5] = {0};               // UID de la tarjeta leída
char hex[3];

// ---------------- Servo (Talanquera) ----------------
void Servo_Init(void) {
    DDRB |= (1<<PB1);  // OC1A como salida (PB1)
    TCCR1A = (1<<COM1A1) | (1<<WGM11);          // Modo PWM no invertido
    TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11); // Prescaler 8, modo 14 (Fast PWM ICR1 TOP)
    ICR1 = 39999;  // Frecuencia 50Hz (20ms período)
}

void Servo_Abrir(void) {
    // Pulso 2ms ? posición abierta
    OCR1A = 4000;
}

void Servo_Cerrar(void) {
    // Pulso 1ms ? posición cerrada
    OCR1A = 2000;
}

// ---------------- NFC ----------------
void lectura_NFC(void) {
    if (rfid_isCard()) {
        UART_write_txt("Tarjeta detectada!\r\n");
        if (rfid_readCardSerial()) {
            UART_write_txt("UID: ");
            for (int i = 0; i < 5; i++) {
                uid[i] = rfid_state.serNum[i]; // guardar UID
                sprintf(hex, "%02X", uid[i]);
                UART_write_txt(hex);
            }
            UART_write_txt("\r\n");

            // Aquí se podría validar UID contra lista autorizada
            // if (uid[0]==0xDE && uid[1]==0xAD && ...) { // ejemplo filtro
            //     Servo_Abrir();
            // }

            // Por ahora cualquier tarjeta abre talanquera
            Servo_Abrir();
            _delay_ms(3000);   // tiempo de paso
            Servo_Cerrar();
        }
    }
    _delay_ms(200);
}

// ---------------- I2C ISR ----------------
ISR(TWI_vect) {
    uint8_t estado;
    static uint8_t i = 0;  // índice de envío UID
    estado = TWSR & 0xFC;

    switch (estado) {
        case 0x60: // SLA+W recibido
        case 0x70:
            TWCR |= (1<<TWINT);
            break;

        case 0x80: // Dato recibido desde Maestro
        case 0x90:
            buffer = TWDR;  // Guardamos comando
            TWCR |= (1 << TWINT);
            break;

        case 0xA8: // SLA+R recibido -> enviar datos
        case 0xB8:
            if (buffer == 'R') {
                TWDR = uid[i++];
                if (i >= 5) i = 0; // reinicia después de enviar 5 bytes
            } else {
                TWDR = 0xFF; // Valor inválido
            }
            TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
            break;

        default:
            TWCR |= (1 << TWINT) | (1 << TWSTO);
            break;
    }
}

// ---------------- MAIN ----------------
int main(void) {
    UART_init();
    I2C_Slave_Init(SlaveAddress);
    rfid_init();
    Servo_Init();

    sei(); // Habilitar interrupciones

    Servo_Cerrar(); // iniciar cerrada
    UART_write_txt("\r\nEsclavo I2C - NFC listo!\r\n");

    while (1) {
        lectura_NFC();   // Leer tarjetas
    }
}
