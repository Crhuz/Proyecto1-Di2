#include "spi_simple.h"

void spi_master_init(void) {
	// Configurar pines: MOSI, SCK, SS como salidas; MISO como entrada
	DDRB |= (1 << DDB3) | (1 << DDB5) | (1 << DDB2);  // MOSI, SCK, SS
	DDRB &= ~(1 << DDB4);                              // MISO
	
	// Configurar SPI: Habilitar, Master, prescaler 16, MSB first
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
	
	// SS alto inicialmente (inactivo)
	PORTB |= (1 << PORTB2);
}

void spi_slave_init(void) {
	// Configurar pines: MISO como salida; MOSI, SCK, SS como entradas
	DDRB |= (1 << DDB4);                               // MISO
	DDRB &= ~((1 << DDB3) | (1 << DDB5) | (1 << DDB2)); // MOSI, SCK, SS
	
	// Configurar SPI: Habilitar, Slave
	SPCR = (1 << SPE);
}

uint8_t spi_transceive(uint8_t data) {
	SPDR = data;                        // Enviar dato
	while (!(SPSR & (1 << SPIF)));      // Esperar a que termine
	return SPDR;                        // Retornar dato recibido
}

uint8_t spi_data_ready(void) {
	return (SPSR & (1 << SPIF));        // Verificar si hay dato listo
}