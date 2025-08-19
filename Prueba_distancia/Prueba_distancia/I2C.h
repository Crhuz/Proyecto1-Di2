/*
 * IncFile1.h
 *
 * Created: 1/08/2025 07:13:11
 *  Author: valen
 */ 


#ifndef INCFILE1_H_
#define INCFILE1_H_

#include <avr/io.h>
#include <stdint.h>

// Funcion para inicializar I2C Maestro
void I2C_Master_Init(unsigned long SCL_Clock, uint8_t Prescaler);

// Funcion de inicio de la comunicacion I2C
void I2C_Master_Start(void);

// Funcion de parada de la comunicacion I2C
void I2C_Master_Stop(void);

// Funcion de transmision de datos del maestro al esclavo
// (Devolvera un 0 si el esclavo ha recibido el dato)
uint8_t I2C_Master_Write(uint8_t dato);

// Funcion de recepcion de datos enviados por el esclavo al maestro
// (Lee los datos que estan en el esclavo)
uint8_t I2C_Master_Read(uint8_t *buffer, uint8_t ack);

// Funcion para inicializar I2C Esclavo
void I2C_Slave_Init(uint8_t address);



#endif /* INCFILE1_H_ */