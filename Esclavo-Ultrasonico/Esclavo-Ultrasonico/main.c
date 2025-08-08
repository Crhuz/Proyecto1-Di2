/*
 * Ultrasonico_UART.c
 *
 * Created: 08/08/2025
 * Author: Manuel
 * Description: Medición de distancia con sensor HC-SR04 usando ATmega328P,
 *              Trig en PC4, Echo en PC3, envío por UART.
 */

#define F_CPU 16000000UL // Frecuencia del CPU (16 MHz)
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define F_CPU 16000000UL   // Frecuencia de CPU
#define BAUD 9600          // Velocidad UART
#define MYUBRR F_CPU/16/BAUD-1

// Pines del sensor
#define TRIG_PORT PORTC
#define TRIG_DDR  DDRC
#define TRIG_PIN  PC4

#define ECHO_PIN  PC3
#define ECHO_PORT PINC

// Prototipos
void UART_Init(unsigned int ubrr);
void UART_SendChar(char c);
void UART_SendString(const char* str);
void Ultrasonico_Init(void);
uint16_t Ultrasonico_Read(void);

int main(void)
{
    char buffer[32];
    uint16_t distancia;

    UART_Init(MYUBRR);
    Ultrasonico_Init();

    UART_SendString("Iniciando medicion ultrasonico...\r\n");

    while (1)
    {
        distancia = Ultrasonico_Read();  // Obtener distancia en cm
        sprintf(buffer, "Distancia: %u cm\r\n", distancia);
        UART_SendString(buffer);
        _delay_ms(500);
    }
}

// ---------------- UART ----------------
void UART_Init(unsigned int ubrr)
{
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1<<TXEN0);                        // Habilitar transmisión
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);         // 8 bits de datos, 1 bit de stop
}

void UART_SendChar(char c)
{
    while (!(UCSR0A & (1<<UDRE0)));  // Esperar buffer vacío
    UDR0 = c;
}

void UART_SendString(const char* str)
{
    while (*str)
    {
        UART_SendChar(*str++);
    }
}

// ---------------- Ultrasonico ----------------
void Ultrasonico_Init(void)
{
    TRIG_DDR |= (1<<TRIG_PIN);   // TRIG como salida
    DDRC &= ~(1<<ECHO_PIN);      // ECHO como entrada
}

uint16_t Ultrasonico_Read(void)
{
    uint16_t contador = 0;

    // Pulso TRIG de 10us
    TRIG_PORT &= ~(1<<TRIG_PIN);
    _delay_us(2);
    TRIG_PORT |= (1<<TRIG_PIN);
    _delay_us(10);
    TRIG_PORT &= ~(1<<TRIG_PIN);

    // Esperar a que ECHO se ponga en alto
    while (!(ECHO_PORT & (1<<ECHO_PIN)));

    // Medir ancho de pulso usando un contador
    while (ECHO_PORT & (1<<ECHO_PIN))
    {
        _delay_us(1);
        contador++;
    }

    // Convertir a cm: velocidad del sonido ? 0.0343 cm/us, ida y vuelta /2
    return (contador * 0.0343) / 2;
}
