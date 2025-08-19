/*
 * Maestro_Peso_Distancia_NFC_LCD_UART.c
 *
 * Created: 18/08/2025
 * Author: Manuel
 * Description:
 *   Maestro I2C que:
 *   - Lee peso desde esclavo 0x20 (2 bytes: entero y decimal)
 *   - Lee distancia y primer byte de NFC desde esclavo 0x30
 *   - Recibe desde ESP32 un valor E:0-4 por UART
 *   - Muestra todo en LCD
 *   - Envía valor E al esclavo NFC/Distancia
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "LCD.h"
#include "I2C.h"

// ========================
// Configuración
// ========================
#define SLAVE_PESO      0x20
#define SLAVE_DISTANCIA 0x30
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

// ========================
// UART
// ========================
void UART_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1<<TXEN0) | (1<<RXEN0);   // TX + RX
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);   // 8 bits, 1 stop
}

void UART_SendChar(char data) {
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

void UART_SendString(char *s) {
    while (*s) {
        UART_SendChar(*s++);
    }
}

char UART_ReadChar(void) {
    while (!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

// ========================
// MAIN
// ========================
int main(void)
{
    char buffer[16];
    uint8_t entera = 0, decimal = 0;
    uint8_t distancia = 0, nfc_byte = 0;
    int elevadorNivel = -1;   // -1 = sin valor

    lcd_init();
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_string("Iniciando...");
    _delay_ms(2000);

    UART_Init(MYUBRR);

    // Configuración pines I2C
    DDRC &= ~((1<<PC4)|(1<<PC5));
    PORTC |= (1<<PC4)|(1<<PC5);
    I2C_Master_Init(50000, 1);

    while (1)
    {
        // ====================
        // 1) LEER PESO
        // ====================
        I2C_Master_Start();
        I2C_Master_Write((SLAVE_PESO << 1) | 0x00);
        I2C_Master_Write('R');
        I2C_Master_Stop();

        _delay_ms(5);

        I2C_Master_Start();
        I2C_Master_Write((SLAVE_PESO << 1) | 0x01);

        // Byte entero
        TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
        while(!(TWCR & (1<<TWINT)));
        entera = TWDR;

        // Byte decimal
        TWCR = (1<<TWINT)|(1<<TWEN);
        while(!(TWCR & (1<<TWINT)));
        decimal = TWDR;

        I2C_Master_Stop();

        // ====================
        // LEER DISTANCIA
        // ====================
        I2C_Master_Start();
        I2C_Master_Write((SLAVE_DISTANCIA << 1) | 0x00);
        I2C_Master_Write('D');     // <<-- Pedimos distancia
        I2C_Master_Stop();

        _delay_ms(5);

        I2C_Master_Start();
        I2C_Master_Write((SLAVE_DISTANCIA << 1) | 0x01);
        TWCR = (1<<TWINT)|(1<<TWEN);
        while(!(TWCR & (1<<TWINT)));
        distancia = TWDR;
        I2C_Master_Stop();

        // ====================
        // LEER NFC
        // ====================
        I2C_Master_Start();
        I2C_Master_Write((SLAVE_DISTANCIA << 1) | 0x00);
        I2C_Master_Write('R');     // <<-- Pedimos NFC
        I2C_Master_Stop();

        _delay_ms(5);

        I2C_Master_Start();
        I2C_Master_Write((SLAVE_DISTANCIA << 1) | 0x01);
        TWCR = (1<<TWINT)|(1<<TWEN);
        while(!(TWCR & (1<<TWINT)));
        nfc_byte = 79;
        I2C_Master_Stop();
		
		// ====================
		// REENVIAR NFC ? esclavo peso
		// ====================
		I2C_Master_Start();
		I2C_Master_Write((SLAVE_PESO << 1) | 0x00);
		I2C_Master_Write('N');      // Comando: enviar NFC
		I2C_Master_Write(nfc_byte); // Valor NFC
		I2C_Master_Stop();

        // ====================
        // REVISAR UART (ESP32 -> Maestro)
        // ====================
        if (UCSR0A & (1<<RXC0)) {
            char c = UART_ReadChar();
            static char uart_buffer[10];
            static uint8_t idx = 0;

            if (c == '\n' || c == '\r') {
                uart_buffer[idx] = '\0';
                if (strncmp(uart_buffer, "E:", 2) == 0) {
                    elevadorNivel = atoi(&uart_buffer[2]);

                    // Mandar al esclavo distancia/NFC
                    I2C_Master_Start();
                    I2C_Master_Write((SLAVE_DISTANCIA << 1) | 0x00);
                    I2C_Master_Write('E');         // Comando elevador
                    I2C_Master_Write(elevadorNivel); // Nivel 0–4
                    I2C_Master_Stop();
                }
                idx = 0; // Reiniciar buffer
            } else {
                if (idx < sizeof(uart_buffer)-1) {
                    uart_buffer[idx++] = c;
                }
            }
        }

        // ====================
        // MOSTRAR EN LCD
        // ====================
        lcd_clear();

        lcd_set_cursor(0, 0);
        lcd_string("P:");
        itoa(entera, buffer, 10);
        lcd_string(buffer);
        lcd_string(".");
        if(decimal < 10) lcd_string("0");
        itoa(decimal, buffer, 10);
        lcd_string(buffer);
        lcd_string("g");

        lcd_set_cursor(1, 0);
        lcd_string("D:");
        itoa(distancia, buffer, 10);
        lcd_string(buffer);
        lcd_string(" N:");
        itoa(nfc_byte, buffer, 10);
        lcd_string(buffer);


        lcd_set_cursor(0, 11);
        lcd_string("E:");
        itoa(elevadorNivel, buffer, 10);
        lcd_string(buffer);


        // ====================
        // ENVIAR POR UART
        // ====================
        char uart_msg[40];
		
        sprintf(uart_msg, "P:%d.%02d g\n", 100, 20);
        UART_SendString(uart_msg);
		_delay_ms(100); // Evita saturar
        sprintf(uart_msg, "D:%d cm\n", distancia);
        UART_SendString(uart_msg);
		_delay_ms(100); // Evita saturar
		sprintf(uart_msg, "N:%d\n", nfc_byte);
		UART_SendString(uart_msg);
        _delay_ms(100); // Evita saturar
    }
}
