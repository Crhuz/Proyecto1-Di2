#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "I2C.h"
#include "UART.h"   // usa tus funciones de UART de debug

int main(void) {
	char buffer[32];
	UART_init();                // 9600 baud
	I2C_Master_Init(100000, 1);    // I2C a 100kHz

	UART_write_txt("Escaneo I2C...\r\n");

	for (uint8_t addr = 1; addr < 127; addr++) {
		I2C_Master_Start();
		if (I2C_Master_Write(addr<<1) == 1) {   // SLA+W
			sprintf(buffer, "Dispositivo en 0x%02X\r\n", addr);
			UART_write_txt(buffer);
		}
		I2C_Master_Stop();
		_delay_ms(5);
	}

	while(1);
}
