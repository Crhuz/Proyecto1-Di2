/*
 * NFC_Ultrasonico_I2C_Slave.c
 *
 * Created: 19/08/2025
 * Author: Manuel
 * Description:
 *   Esclavo I2C (0x30) que combina:
 *      - Lector RFID RC522 (NFC)
 *      - Sensor Ultrasonico HC-SR04
 *      - Motor (elevador)
 *   Envía por I2C el valor solicitado por el maestro:
 *      'D' ? distancia en cm
 *      'R' ? primer byte del UID NFC
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

// Pines del ultrasonico
#define TRIG_PORT PORTD
#define TRIG_DDR  DDRD
#define TRIG_PIN  PD5
#define ECHO_PIN  PD6
#define ECHO_PORT PIND

// ================= Puente H =================
#define H_DDR   DDRB
#define H_PORT  PORTB
#define IN1     PB0
#define IN2     PB1


// ---------------- Variables ----------------
volatile uint8_t buffer = 0;       // Comando recibido del Maestro
volatile uint8_t distancia_cm = 0; // Última distancia medida
volatile uint8_t ntarjeta = 0;     // Primer byte del UID NFC

char uart_buf[32];
char hex[3];

uint16_t Ultrasonico_Read(void);

// ---------------- Motor ----------------

void Motor_Init(void) {
	H_DDR |= (1<<IN1) | (1<<IN2);
}

void Motor_Stop(void) {
	H_PORT &= ~((1<<IN1)|(1<<IN2));
	OCR0A = 0;
}

void Motor_Up(void) {
	H_PORT |= (1<<IN1);
	H_PORT &= ~(1<<IN2);
	OCR0A = 200;
}

void Motor_Down(void) {
	H_PORT |= (1<<IN2);
	H_PORT &= ~(1<<IN1);
	OCR0A = 200;
}

// ================= Control de niveles =================
uint8_t nivel_objetivo = 0;

void MoverANivel(uint8_t nivel) {
	uint8_t target_cm = 0;
	switch(nivel) {
		case 1: target_cm = 22; break;
		case 2: target_cm = 14; break;
		case 3: target_cm = 8;  break;
		case 4: target_cm = 0;  break;
		default: return; // nivel inválido
	}

	sprintf(uart_buf, "Moviendo a nivel %u (%u cm)\r\n", nivel, target_cm);
	UART_write_txt(uart_buf);

	while (1) {
		uint16_t d = Ultrasonico_Read();
		distancia_cm = (d>255)?255:d;

		if (distancia_cm > target_cm + 1) {
			Motor_Down(); // bajar
			} else if (distancia_cm < target_cm - 1) {
			Motor_Up();   // subir
			} else {
			Motor_Stop();
			sprintf(uart_buf, "Nivel %u alcanzado (%u cm)\r\n", nivel, distancia_cm);
			UART_write_txt(uart_buf);
			break;
		}
		_delay_ms(100);
	}
}


// ---------------- Ultrasonico ----------------
void Ultrasonico_Init(void) {
    TRIG_DDR |= (1<<TRIG_PIN);   // TRIG salida
    DDRD &= ~(1<<ECHO_PIN);      // ECHO entrada
}

uint16_t Ultrasonico_Read(void) {
    uint16_t contador = 0;

    // Pulso TRIG 10us
    TRIG_PORT &= ~(1<<TRIG_PIN);
    _delay_us(2);
    TRIG_PORT |= (1<<TRIG_PIN);
    _delay_us(10);
    TRIG_PORT &= ~(1<<TRIG_PIN);

    // Esperar flanco de subida en ECHO
    while (!(ECHO_PORT & (1<<ECHO_PIN)));

    // Medir ancho del pulso
    while (ECHO_PORT & (1<<ECHO_PIN)) {
        _delay_us(1);
        contador++;
    }

    // Convertir a cm
    return (contador * 0.0343) / 2;
}

// ---------------- NFC ----------------

void lectura_NFC(void) {
    if (rfid_isCard()) {
        UART_write_txt("Card detected!\r\n");
        if (rfid_readCardSerial()) {
            UART_write_txt("Card Serial: ");
            for (int i = 0; i < 5; i++) {
                sprintf(hex, "%02X", rfid_state.serNum[i]);
                UART_write_txt(hex);
            }
            ntarjeta = rfid_state.serNum[0]; // Guardamos primer byte
            UART_write_txt("\r\n");

        } else {
            UART_write_txt("Failed to read card serial.\r\n");
        }
    }
    _delay_ms(200);
}

// ---------------- I2C ISR ----------------
ISR(TWI_vect) {
    uint8_t estado;
    estado = TWSR & 0xFC;

    switch (estado) {
        case 0x60: // SLA+W recibido
        case 0x70:
            TWCR |= (1<<TWINT);
            break;

        case 0x80: // Dato recibido desde Maestro
        case 0x90:
        if (buffer == 'E') {
	        // el siguiente byte es el nivel
	        nivel_objetivo = TWDR;
	        MoverANivel(nivel_objetivo);
	        } else {
	        buffer = TWDR;  // Guardamos comando normal ('D','R')
        }
        TWCR |= (1 << TWINT);
        break;

        case 0xA8: // SLA+R recibido ? enviar dato
        case 0xB8:
            if (buffer == 'D') {
                TWDR = distancia_cm;  
            } else if (buffer == 'R') {
                TWDR = ntarjeta;
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
    Ultrasonico_Init();
    I2C_Slave_Init(SlaveAddress);
    rfid_init();
	Motor_Init();
	

    sei(); // Habilitar interrupciones

    UART_write_txt("\r\nEsclavo I2C - NFC + Ultrasonico + Servo listo!\r\n");

  while (1) {
	  // Actualizar ultrasonico
	  uint16_t d = Ultrasonico_Read();
	  if (d > 255) d = 255;
	  distancia_cm = (uint8_t)d;
	  sprintf(uart_buf, "Distancia: %u cm\r\n", distancia_cm);
	  UART_write_txt(uart_buf);
	  // Actualizar NFC }
	  lectura_NFC();
	  _delay_ms(500);
  }
}
