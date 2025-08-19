#include "ALB_RFID.h"

// Instancia global del estado RFID
RFID_State_t rfid_state;

/******************************************************************************
 * Implementación de Funciones de Bajo Nivel (Pines y SPI)
 ******************************************************************************/

// Inicialización de pines (reemplazo de pinMode)
void rfid_pin_init(void) {
    // Configurar pines CS y RST como salida
    RFID_CS_DDR |= (1 << RFID_CS_PIN);
    RFID_RST_DDR |= (1 << RFID_RST_PIN);

    // Inicializar CS en HIGH (inactivo) y RST en HIGH (activo)
    RFID_CS_PORT |= (1 << RFID_CS_PIN);
    RFID_RST_PORT |= (1 << RFID_RST_PIN);
}

// Funciones para controlar el pin Chip Select (CS)
void rfid_cs_low(void) {
    RFID_CS_PORT &= ~(1 << RFID_CS_PIN);
}

void rfid_cs_high(void) {
    RFID_CS_PORT |= (1 << RFID_CS_PIN);
}

// Funciones para controlar el pin Reset (NRSTPD)
void rfid_rst_low(void) {
    RFID_RST_PORT &= ~(1 << RFID_RST_PIN);
}

void rfid_rst_high(void) {
    RFID_RST_PORT |= (1 << RFID_RST_PIN);
}

// Implementación básica de SPI para ATmega328P
void spi_init(void) {
    // Configurar MOSI, SCK, SS (manual) como salida, MISO como entrada
    SPI_DDR |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_SS);
    SPI_DDR &= ~(1 << SPI_MISO);

    // Habilitar SPI, Maestro, SCK Fosc/16 (ajusta según necesidad)
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); // SPR1 y SPR0 para velocidad
    SPSR = 0; // Asegurarse de que SPI2X esté en 0 para Fosc/16
}

unsigned char spi_transfer(unsigned char data) {
    SPDR = data; // Cargar datos en el registro de datos SPI
    while (!(SPSR & (1 << SPIF))); // Esperar a que la transferencia se complete
    return SPDR; // Devolver los datos recibidos
}

/******************************************************************************
 * Implementación de la Librería RFID
 ******************************************************************************/

// Constructor (ahora función de inicialización)
void rfid_init(void) {
    rfid_pin_init(); // Inicializar pines CS y RST
    spi_init();      // Inicializar SPI

    rfid_rst_high(); // Asegurarse de que el pin de reset esté alto

    rfid_reset();

    // Timer: TPrescaler*TreloadVal/6.78MHz = 24ms
    rfid_writeMFRC522(TModeReg, 0x8D);      // Tauto=1; f(Timer) = 6.78MHz/TPreScaler
    rfid_writeMFRC522(TPrescalerReg, 0x3E); // TModeReg[3..0] + TPrescalerReg
    rfid_writeMFRC522(TReloadRegL, 30);
    rfid_writeMFRC522(TReloadRegH, 0);

    rfid_writeMFRC522(TxAutoReg, 0x40);     // 100%ASK
    rfid_writeMFRC522(ModeReg, 0x3D);       // CRC valor inicial de 0x6363

    rfid_antennaOn(); // Abre la antena
}

void rfid_reset(void) {
    rfid_writeMFRC522(CommandReg, PCD_RESETPHASE);
}

void rfid_writeMFRC522(unsigned char addr, unsigned char val) {
    rfid_cs_low();

    // 0XXXXXX0 formato de dirección
    spi_transfer((addr << 1) & 0x7E);
    spi_transfer(val);

    rfid_cs_high();
}

void rfid_antennaOn(void) {
    unsigned char temp;

    temp = rfid_readMFRC522(TxControlReg);
    if (!(temp & 0x03)) {
        rfid_setBitMask(TxControlReg, 0x03);
    }
}

unsigned char rfid_readMFRC522(unsigned char addr) {
    unsigned char val;
    rfid_cs_low();
    spi_transfer(((addr << 1) & 0x7E) | 0x80);
    val = spi_transfer(0x00);
    rfid_cs_high();
    return val;
}

void rfid_setBitMask(unsigned char reg, unsigned char mask) {
    unsigned char tmp;
    tmp = rfid_readMFRC522(reg);
    rfid_writeMFRC522(reg, tmp | mask);  // set bit mask
}

void rfid_clearBitMask(unsigned char reg, unsigned char mask) {
    unsigned char tmp;
    tmp = rfid_readMFRC522(reg);
    rfid_writeMFRC522(reg, tmp & (~mask));  // clear bit mask
}

void rfid_calculateCRC(unsigned char *pIndata, unsigned char len, unsigned char *pOutData) {
    unsigned char i, n;

    rfid_clearBitMask(DivIrqReg, 0x04);         // CRCIrq = 0
    rfid_setBitMask(FIFOLevelReg, 0x80);        // Claro puntero FIFO

    // Escribir datos en el FIFO
    for (i = 0; i < len; i++) {
        rfid_writeMFRC522(FIFODataReg, *(pIndata + i));
    }
    rfid_writeMFRC522(CommandReg, PCD_CALCCRC);

    // Esperar a la finalización de cálculo del CRC
    i = 0xFF;
    do {
        n = rfid_readMFRC522(DivIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x04)); // CRCIrq = 1

    // Lea el cálculo de CRC
    pOutData[0] = rfid_readMFRC522(CRCResultRegL);
    pOutData[1] = rfid_readMFRC522(CRCResultRegM);
}

unsigned char rfid_MFRC522ToCard(unsigned char command, unsigned char *sendData, unsigned char sendLen, unsigned char *backData, unsigned int *backLen) {
    unsigned char status = MI_ERR;
    unsigned char irqEn = 0x00;
    unsigned char waitIRq = 0x00;
    unsigned char lastBits;
    unsigned char n;
    unsigned int i;

    switch (command) {
        case PCD_AUTHENT: // Tarjetas de certificación cerca
        {
            irqEn = 0x12;
            waitIRq = 0x10;
            break;
        }
        case PCD_TRANSCEIVE: // La transmisión de datos FIFO
        {
            irqEn = 0x77;
            waitIRq = 0x30;
            break;
        }
        default:
            break;
    }

    rfid_writeMFRC522(CommIEnReg, irqEn | 0x80); // De solicitud de interrupción
    rfid_clearBitMask(CommIrqReg, 0x80);         // Borrar todos los bits de petición de interrupción
    rfid_setBitMask(FIFOLevelReg, 0x80);         // FlushBuffer=1, FIFO de inicialización

    rfid_writeMFRC522(CommandReg, PCD_IDLE); // NO action; Y cancelar el comando

    // Escribir datos en el FIFO
    for (i = 0; i < sendLen; i++) {
        rfid_writeMFRC522(FIFODataReg, sendData[i]);
    }

    // Ejecutar el comando
    rfid_writeMFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        rfid_setBitMask(BitFramingReg, 0x80); // StartSend=1,transmission of data starts
    }

    // A la espera de recibir datos para completar
    i = 2000; // Tiempo máximo de espera operación M1 25ms tarjeta
    do {
        n = rfid_readMFRC522(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    rfid_clearBitMask(BitFramingReg, 0x80); // StartSend=0

    if (i != 0) {
        if (!(rfid_readMFRC522(ErrorReg) & 0x1B)) // BufferOvfl Collerr CRCErr ProtecolErr
        {
            status = MI_OK;
            if (n & irqEn & 0x01) {
                status = MI_NOTAGERR;
            }

            if (command == PCD_TRANSCEIVE) {
                n = rfid_readMFRC522(FIFOLevelReg);
                lastBits = rfid_readMFRC522(ControlReg) & 0x07;
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }

                if (n == 0) {
                    n = 1;
                }
                if (n > MAX_LEN) {
                    n = MAX_LEN;
                }

                // Lea los datos recibidos en el FIFO
                for (i = 0; i < n; i++) {
                    backData[i] = rfid_readMFRC522(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}

unsigned char rfid_MFRC522Request(unsigned char reqMode, unsigned char *TagType) {
    unsigned char status;
    unsigned int backBits; // Recibió bits de datos

    rfid_writeMFRC522(BitFramingReg, 0x07); // TxLastBists = BitFramingReg[2..0]

    TagType[0] = reqMode;
    status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }

    return status;
}

unsigned char rfid_anticoll(unsigned char *serNum_out) {
    unsigned char status;
    unsigned char i;
    unsigned char serNumCheck = 0;
    unsigned int unLen;

    rfid_writeMFRC522(BitFramingReg, 0x00); // TxLastBists = BitFramingReg[2..0]

    serNum_out[0] = PICC_ANTICOLL;
    serNum_out[1] = 0x20;
    status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, serNum_out, 2, serNum_out, &unLen);

    if (status == MI_OK) {
        // Compruebe el número de serie de la tarjeta
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum_out[i];
        }
        if (serNumCheck != serNum_out[i]) {
            status = MI_ERR;
        }
    }

    return status;
}

unsigned char rfid_auth(unsigned char authMode, unsigned char BlockAddr, unsigned char *Sectorkey, unsigned char *serNum_in) {
    unsigned char status;
    unsigned int recvBits;
    unsigned char i;
    unsigned char buff[12];

    // Verifique la dirección de comandos de bloques del sector + + contraseña + número de la tarjeta de serie
    buff[0] = authMode;
    buff[1] = BlockAddr;
    for (i = 0; i < 6; i++) {
        buff[i + 2] = *(Sectorkey + i);
    }
    for (i = 0; i < 4; i++) {
        buff[i + 8] = *(serNum_in + i);
    }
    status = rfid_MFRC522ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);

    if ((status != MI_OK) || (!(rfid_readMFRC522(Status2Reg) & 0x08))) {
        status = MI_ERR;
    }

    return status;
}

unsigned char rfid_read(unsigned char blockAddr, unsigned char *recvData) {
    unsigned char status;
    unsigned int unLen;

    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;
    rfid_calculateCRC(recvData, 2, &recvData[2]);
    status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

    if ((status != MI_OK) || (unLen != 0x90)) {
        status = MI_ERR;
    }

    return status;
}

unsigned char rfid_write(unsigned char blockAddr, unsigned char *writeData) {
    unsigned char status;
    unsigned int recvBits;
    unsigned char i;
    unsigned char buff[18];

    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    rfid_calculateCRC(buff, 2, &buff[2]);
    status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

    if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) {
        status = MI_ERR;
    }

    if (status == MI_OK) {
        for (i = 0; i < 16; i++) // Datos a la FIFO 16Byte escribir
        {
            buff[i] = *(writeData + i);
        }
        rfid_calculateCRC(buff, 16, &buff[16]);
        status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);

        if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) {
            status = MI_ERR;
        }
    }

    return status;
}

void rfid_halt(void) {
    unsigned char status;
    unsigned int unLen;
    unsigned char buff[4];

    buff[0] = PICC_HALT;
    buff[1] = 0;
    rfid_calculateCRC(buff, 2, &buff[2]);

    status = rfid_MFRC522ToCard(PCD_TRANSCEIVE, buff, 4, buff, &unLen);
}

/******************************************************************************
 * Funciones de Alto Nivel (User API)
 ******************************************************************************/

unsigned char rfid_isCard(void) {
    unsigned char status;
    unsigned char str[MAX_LEN];

    status = rfid_MFRC522Request(PICC_REQIDL, str);
    if (status == MI_OK) {
        return 1; // true
    } else {
        return 0; // false
    }
}

unsigned char rfid_readCardSerial(void) {
    unsigned char status;
    unsigned char str[MAX_LEN];

    // Anti-colisión, devuelva el número de serie de tarjeta de 4 bytes
    status = rfid_anticoll(str);
    memcpy(rfid_state.serNum, str, 5); // Copiar el número de serie a la estructura global

    if (status == MI_OK) {
        return 1; // true
    } else {
        return 0; // false
    }
}
