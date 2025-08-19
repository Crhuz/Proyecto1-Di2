#ifndef RFID_H
#define RFID_H

#include <avr/io.h>     // Para registros del microcontrolador (DDRx, PORTx, PINx)
#include <util/delay.h> // Para funciones de retardo como _delay_ms
#include <string.h>     // Para memcpy

// Si ALB_SPI.h no es una librería AVR pura, necesitarás implementar las funciones SPI directamente aquí
// o incluir la cabecera SPI adecuada para AVR.
// Por ejemplo, si usas la librería SPI de Arduino, necesitarías adaptarla o escribir tu propia implementación.
// Para este ejemplo, asumiremos que las funciones SPI (spi_init, spi_transfer) se implementarán o ya existen.
// #include "ALB_SPI.h" // Si tienes una versión C de ALB_SPI.h

/******************************************************************************
 * Definiciones de Pines (Ejemplo - Ajusta según tu hardware)
 ******************************************************************************/
#define RFID_CS_PORT PORTB
#define RFID_CS_DDR  DDRB
#define RFID_CS_PIN  PB2 // Pin de Chip Select (SS)

#define RFID_RST_PORT PORTD
#define RFID_RST_DDR  DDRD
#define RFID_RST_PIN  PD7 // Pin de Reset (NRSTPD)

/******************************************************************************
 * Definiciones de SPI (Ejemplo - Ajusta según tu hardware)
 ******************************************************************************/
#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define SPI_MISO PB4
#define SPI_MOSI PB3
#define SPI_SCK  PB5
#define SPI_SS   PB2 // Chip Select, aunque lo manejamos manualmente

/******************************************************************************
 * Definiciones
 ******************************************************************************/
#define MAX_LEN 16   // Largo máximo de la matriz

//MF522 comando palabra
#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

//Mifare_One  Tarjeta Mifare_One comando palabra
#define PICC_REQIDL           0x26
#define PICC_REQALL           0x52
#define PICC_ANTICOLL         0x93
#define PICC_SElECTTAG        0x93
#define PICC_AUTHENT1A        0x60
#define PICC_AUTHENT1B        0x61
#define PICC_READ             0x30
#define PICC_WRITE            0xA0
#define PICC_DECREMENT        0xC0
#define PICC_INCREMENT        0xC1
#define PICC_RESTORE          0xC2
#define PICC_TRANSFER         0xB0
#define PICC_HALT             0x50

//MF522 Código de error de comunicación cuando regresó
#define MI_OK                 0
#define MI_NOTAGERR           1
#define MI_ERR                2

//------------------ MFRC522 registro---------------
//Page 0:Command and Status
#define     Reserved00            0x00
#define     CommandReg            0x01
#define     CommIEnReg            0x02
#define     DivlEnReg             0x03
#define     CommIrqReg            0x04
#define     DivIrqReg             0x05
#define     ErrorReg              0x06
#define     Status1Reg            0x07
#define     Status2Reg            0x08
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     Reserved01            0x0F
//Page 1:Command
#define     Reserved10            0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     Reserved11            0x1A
#define     Reserved12            0x1B
#define     MifareReg             0x1C
#define     Reserved13            0x1D
#define     Reserved14            0x1E
#define     SerialSpeedReg        0x1F
//Page 2:CFG
#define     Reserved20            0x20
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     Reserved21            0x23
#define     ModWidthReg           0x24
#define     Reserved22            0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsPReg	          0x28
#define     ModGsPReg             0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F
//Page 3:TestRegister
#define     Reserved30            0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39
#define     TestDAC2Reg           0x3A
#define     TestADCReg            0x3B
#define     Reserved31            0x3C
#define     Reserved32            0x3D
#define     Reserved33            0x3E
#define     Reserved34			  0x3F
//-----------------------------------------------

// Estructura para mantener el estado del RFID, similar a los miembros de la clase en C++
typedef struct {
    // Los pines CS y RST se manejarán directamente con macros de puerto/pin
    unsigned char serNum[5];       // Constante para guardar el numero de serie leido.
    unsigned char AserNum[5];      // Constante para guardar el numero d serie de la sesion actual.
} RFID_State_t;

// Declaración de la instancia global del estado RFID
extern RFID_State_t rfid_state;

/******************************************************************************
 * Prototipos de Funciones
 ******************************************************************************/

// Funciones de bajo nivel para manipulación de pines (reemplazo de Arduino pinMode/digitalWrite)
void rfid_pin_init(void);
void rfid_cs_low(void);
void rfid_cs_high(void);
void rfid_rst_low(void);
void rfid_rst_high(void);

// Funciones SPI (deberás implementarlas o usar una librería SPI para AVR)
void spi_init(void);
unsigned char spi_transfer(unsigned char data);

// Funciones de la librería RFID
void rfid_init(void);
void rfid_reset(void);
void rfid_writeMFRC522(unsigned char addr, unsigned char val);
void rfid_antennaOn(void);
unsigned char rfid_readMFRC522(unsigned char addr);
void rfid_setBitMask(unsigned char reg, unsigned char mask);
void rfid_clearBitMask(unsigned char reg, unsigned char mask);
void rfid_calculateCRC(unsigned char *pIndata, unsigned char len, unsigned char *pOutData);
unsigned char rfid_MFRC522ToCard(unsigned char command, unsigned char *sendData, unsigned char sendLen, unsigned char *backData, unsigned int *backLen);
unsigned char rfid_MFRC522Request(unsigned char reqMode, unsigned char *TagType);
unsigned char rfid_anticoll(unsigned char *serNum_out); // Cambiado el nombre del parámetro para evitar conflicto con rfid_state.serNum
unsigned char rfid_auth(unsigned char authMode, unsigned char BlockAddr, unsigned char *Sectorkey, unsigned char *serNum_in); // Cambiado el nombre del parámetro
unsigned char rfid_read(unsigned char blockAddr, unsigned char *recvData);
unsigned char rfid_write(unsigned char blockAddr, unsigned char *writeData);
void rfid_halt(void);

// Funciones de alto nivel
unsigned char rfid_isCard(void);
unsigned char rfid_readCardSerial(void);

#endif // RFID_H
