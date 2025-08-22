/*
 * Maestro_Peso_NFC_LCD_UART.c
 *
 * Author: Manuel
 * Fecha: 22/08/2025
 *
 * Descripción:
 *   Maestro I2C controlado por ESP32 vía UART:
 *   - "P?"   -> Responde con Peso (lee de esclavo 0x20)
 *   - "N?"   -> Responde con UID NFC en HEX (lee de esclavo 0x30)
 *   - "D?"   -> Responde con nivel/distancia (a integrar con VL53L0X)
 *   - "E:x"  -> Traducir nivel a comando motor 'U'/'D'/'S' y enviarlo al esclavo PESO
 *
 *   LCD muestra Peso, Nivel y UID NFC
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
#define SLAVE_PESO   0x20
#define SLAVE_NFC    0x30

#define BAUD   9600
#define MYUBRR (F_CPU/16/BAUD - 1)

#define UID_LEN 5  // El esclavo NFC envía 5 bytes de UID

// ========================
// UART sencillo
// ========================
static void UART_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);   // RX y TX
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); // 8 bits, 1 stop, sin paridad
}

static inline uint8_t UART_Available(void) {
    return (UCSR0A & (1<<RXC0));
}

static char UART_ReadChar(void) {
    while(!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

static void UART_SendChar(char c){
    while(!(UCSR0A & (1<<UDRE0)));
    UDR0 = c;
}

static void UART_SendString(const char *s){
    while(*s){ UART_SendChar(*s++); }
}

// ========================
// Utilidades
// ========================
static void bytes_to_hex(const uint8_t *in, uint8_t len, char *out){
    static const char hexchars[] = "0123456789ABCDEF";
    for(uint8_t i=0;i<len;i++){
        out[2*i]   = hexchars[(in[i]>>4) & 0x0F];
        out[2*i+1] = hexchars[in[i] & 0x0F];
    }
    out[2*len] = '\0';
}

static int leer_nivel_actual_placeholder(void){
    // TODO: integrar sensor real
    return 0;
}

static void enviar_cmd_motor_peso(char cmd){
    I2C_Master_Start();
    I2C_Master_Write((SLAVE_PESO<<1) | 0x00);
    I2C_Master_Write((uint8_t)cmd);
    I2C_Master_Stop();
}

// ========================
// Lecturas I2C
// ========================
static uint8_t leer_peso(uint8_t *ent, uint8_t *dec){
    uint8_t ok;

    I2C_Master_Start();
    ok = I2C_Master_Write((SLAVE_PESO<<1) | 0x00);
    ok &= I2C_Master_Write('R');
    I2C_Master_Stop();
    if(!ok) return 0;

    _delay_ms(5);

    I2C_Master_Start();
    ok = I2C_Master_Write((SLAVE_PESO<<1) | 0x01);
    if(!ok){ I2C_Master_Stop(); return 0; }

    ok = I2C_Master_Read(ent, 1);
    ok &= I2C_Master_Read(dec, 0);
    I2C_Master_Stop();

    return ok;
}

static uint8_t leer_uid_nfc(uint8_t *uid){
    uint8_t ok;

    I2C_Master_Start();
    ok = I2C_Master_Write((SLAVE_NFC<<1) | 0x00);
    ok &= I2C_Master_Write('R');
    I2C_Master_Stop();
    if(!ok) return 0;

    _delay_ms(5);

    I2C_Master_Start();
    ok = I2C_Master_Write((SLAVE_NFC<<1) | 0x01);
    if(!ok){ I2C_Master_Stop(); return 0; }

    for(uint8_t i=0;i<UID_LEN;i++){
        uint8_t ack = (i < (UID_LEN-1)) ? 1 : 0;
        ok &= I2C_Master_Read(&uid[i], ack);
    }
    I2C_Master_Stop();

    return ok;
}

// ========================
// LCD
// ========================
static void lcd_print_peso(uint8_t ent, uint8_t dec){
    char buf[8];
    lcd_set_cursor(0,0);
    lcd_string("P:");
    itoa(ent, buf, 10); lcd_string(buf);
    lcd_string(".");
    if(dec < 10) lcd_string("0");
    itoa(dec, buf, 10); lcd_string(buf);
    lcd_string("g   ");
}

static void lcd_print_elevador(int nivel){
    char buf[6];
    lcd_set_cursor(0,11);
    lcd_string("E:");
    itoa(nivel, buf, 10);
    lcd_string(buf);
    lcd_string(" ");
}

static void lcd_print_uid_hex(const char *hex10){
    lcd_set_cursor(1,0);
    lcd_string("NFC:");
    lcd_string((char*)hex10);
    uint8_t used = 4 + strlen(hex10);
    for(uint8_t i=used;i<16;i++) lcd_string(" ");
}

// ========================
// MAIN
// ========================
int main(void){
    uint8_t peso_ent=0, peso_dec=0;
    uint8_t uid[UID_LEN] = {0};
    char uid_hex[UID_LEN*2 + 1];
    int elevadorNivel = -1;
    char uart_buf[24];
    uint8_t uart_idx = 0;

    lcd_init();
    lcd_clear();
    lcd_set_cursor(0,0); lcd_string("Iniciando...");
    _delay_ms(800);

    UART_Init(MYUBRR);

    DDRC &= ~((1<<PC4)|(1<<PC5));
    PORTC |= (1<<PC4)|(1<<PC5);
    I2C_Master_Init(50000, 1);

    lcd_clear();
    lcd_set_cursor(0,0); lcd_string("P:--.--g  E:-");
    lcd_set_cursor(1,0); lcd_string("NFC:----------");

    for(;;){
        if (UART_Available()){
            char c = UART_ReadChar();

            if (c=='\n' || c=='\r'){
                uart_buf[uart_idx] = '\0';
                uart_idx = 0;

                // ===== CMD "P?" =====
                if (strcmp(uart_buf, "P?") == 0){
                    if (leer_peso(&peso_ent, &peso_dec)){
                        char out[16];
                        snprintf(out, sizeof(out), "P:%d.%02d\n", peso_ent, peso_dec);
                        UART_SendString(out);
                        lcd_print_peso(peso_ent, peso_dec);
                    } else {
                        UART_SendString("P:ERR\n");
                    }
                }

                // ===== CMD "N?" =====
                else if (strcmp(uart_buf, "N?") == 0){
                    if (leer_uid_nfc(uid)){
                        bytes_to_hex(uid, UID_LEN, uid_hex);
                        UART_SendString("N:");
                        UART_SendString(uid_hex);
                        UART_SendString("\n");
                        lcd_print_uid_hex(uid_hex);
                    } else {
                        UART_SendString("N:ERR\n");
                    }
                }

                // ===== CMD "D?" =====
                else if (strcmp(uart_buf, "D?") == 0){
                    int nivel = leer_nivel_actual_placeholder();
                    char out[12];
                    snprintf(out, sizeof(out), "D:%d\n", nivel);
                    UART_SendString(out);
                }

                // ===== CMD "E:x" =====
                else if (strncmp(uart_buf, "E:", 2) == 0){
                    elevadorNivel = atoi(&uart_buf[2]);
                    lcd_print_elevador(elevadorNivel);

                    int nivel_actual = leer_nivel_actual_placeholder();
                    char cmd = 'S';
                    if (elevadorNivel > nivel_actual)      cmd = 'U';
                    else if (elevadorNivel < nivel_actual) cmd = 'D';
                    enviar_cmd_motor_peso(cmd);

                    char ack[12];
                    snprintf(ack, sizeof(ack), "CMD:%c\n", cmd);
                    UART_SendString(ack);
                }

                // ===== CMD PING =====
                else if (strcmp(uart_buf, "PING") == 0){
                    UART_SendString("PONG\n");
                }

                uart_buf[0] = '\0';
            } else {
                if (uart_idx < sizeof(uart_buf)-1){
                    uart_buf[uart_idx++] = c;
                }
            }
        }
        _delay_ms(10);
    }
}
