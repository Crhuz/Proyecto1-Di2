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
#include "PWM0.h"
#include "I2C.h"

// Dirección I2C del esclavo
#define SlaveAddress 0x30  
#define Talanquera_Abierta 950
#define Talanquera_cerrada 450

// ---------------- Variables ----------------
volatile uint8_t buffer = 0;        // Último comando recibido por I2C
uint8_t uid[5] = {0};               // UID de la tarjeta leída
char hex[3];
uint8_t mservo = 1;

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
               // tiempo de paso
        }
    }
    _delay_ms(500);
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
			if (buffer == 'T'){
				mservo = 1;
			}
            TWCR |= (1 << TWINT);
            break;

        case 0xA8: // SLA+R recibido -> enviar datos
        case 0xB8:
			if (buffer == 'R') {
                TWDR = uid[i++];
                if (i >= 5) i = 0; // reinicia después de enviar 5 bytes
            }else {
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
	PWM0_init();

    sei(); // Habilitar interrupciones

    UART_write_txt("\r\nEsclavo I2C - NFC listo!\r\n");

    while (1) {
        lectura_NFC();   // Leer tarjetas
		
		if (mservo == 1)
		{
			PWM0_dca(Talanquera_Abierta, NO_INVERTING);
			_delay_ms(3000);
			PWM0_dca(Talanquera_cerrada, NO_INVERTING);
			mservo = 0;
		}
    }
}
