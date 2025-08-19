#ifndef SPI_SIMPLE_H
#define SPI_SIMPLE_H

#include <avr/io.h>
#include <stdint.h>

void spi_master_init(void);
void spi_slave_init(void);
uint8_t spi_transceive(uint8_t data);
uint8_t spi_data_ready(void);

#endif