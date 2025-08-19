#ifndef LCD_H
#define LCD_H

#include <avr/io.h>
#include <stdio.h>

// Definiciones de pines para LCD
#define LCD_DATA_PORT PORTD    // Puerto D para datos (PD0-PD7)
#define LCD_DATA_DDR  DDRD     // Dirección del puerto D

#define LCD_CTRL_PORT PORTB    // Puerto B para control
#define LCD_CTRL_DDR  DDRB     // Dirección del puerto B
#define LCD_RS        PB5      // Pin RS en PB0 (Pin 8 Arduino)
#define LCD_E         PB4      // Pin E en PB1 (Pin 9 Arduino)
// RW conectado a GND (solo escritura)

// Comandos LCD HD44780
#define LCD_CLEAR           0x01
#define LCD_HOME            0x02
#define LCD_ENTRY_MODE      0x06
#define LCD_DISPLAY_ON      0x0C
#define LCD_DISPLAY_OFF     0x08
#define LCD_CURSOR_ON       0x0E
#define LCD_CURSOR_BLINK    0x0F
#define LCD_FUNCTION_SET    0x38  // 8-bit, 2 líneas, 5x7 dots
#define LCD_SET_CURSOR      0x80

// Prototipos de funciones
void lcd_init(void);
void lcd_command(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_string(char* str);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_clear(void);
void lcd_send_data(uint8_t data);

#endif