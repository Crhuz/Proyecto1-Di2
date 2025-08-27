/*
 * VL53L0X.h
 *
 * Librería para sensor de distancia VL53L0X
 * Comunicación I2C
 */ 

#ifndef VL53L0X_H_
#define VL53L0X_H_

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include "I2C.h"

// Dirección I2C del VL53L0X (por defecto)
#define VL53L0X_ADDRESS 0x29

// Registros principales del VL53L0X
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID         0xC0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID      0xC2
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD   0x50
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define VL53L0X_REG_SYSRANGE_START                  0x00
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS         0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS             0x14

// Funciones principales
uint8_t VL53L0X_Init(void);
uint8_t VL53L0X_ReadByte(uint8_t reg);
void VL53L0X_WriteByte(uint8_t reg, uint8_t data);
uint16_t VL53L0X_ReadDistance(void);
uint8_t VL53L0X_DataReady(void);
void VL53L0X_StartMeasurement(void);

#endif /* VL53L0X_H_ */