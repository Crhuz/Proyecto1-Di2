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
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include "LCD.h"
#include "I2C.h"
#include "VL53L0X.h"

// ========================
// Configuración
// ========================
#define SLAVE_PESO   0x20
#define SLAVE_NFC    0x30

#define BAUD   9600
#define MYUBRR (F_CPU/16/BAUD - 1)
#define UART_BUF_SIZE 32

#define UID_LEN 5  // El esclavo NFC envía 5 bytes de UID
uint16_t distance = 0;
char buffer[50];
#define AUTHORIZED_COUNT 2 // Número de tarjetas autorizadas

volatile char uart_buf[UART_BUF_SIZE];
volatile uint8_t uart_idx = 0;
volatile int elevadorNivel = 0;

const char *tarjetas_autorizadas[AUTHORIZED_COUNT] = {
	"9AA68D02B3",
	"3353343460"
};

// ========================
// UART sencillo
// ========================
static void UART_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
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

uint8_t es_tarjeta_autorizada(const char *uid_hex) {
	uint8_t ok1 = 1;
	uint8_t autorizada = 0;

	for (int i = 0; i < AUTHORIZED_COUNT; i++) {
		if (strcmp(uid_hex, tarjetas_autorizadas[i]) == 0) {
			// Tarjeta válida
			autorizada = 1;

			// Enviar comando 'T' al esclavo NFC
			I2C_Master_Start();
			ok1 &= I2C_Master_Write((SLAVE_NFC<<1) | 0x00);
			ok1 &= I2C_Master_Write('T');
			I2C_Master_Stop();

			if(!ok1){
				lcd_clear();
				lcd_set_cursor(0,0);
				lcd_string("Error en talanquera");
				_delay_ms(3000);
			}

			lcd_clear();
			lcd_set_cursor(0,0);
			lcd_string("Si autorizado");
			lcd_set_cursor(1,0);
			lcd_string("Pase adelante");
			_delay_ms(3000);
			break; // ya encontramos la tarjeta, no seguir buscando
		}
	}

	// Mostrar talanquera cerrada si no estaba autorizada
	if (!autorizada) {
		lcd_clear();
		lcd_set_cursor(0,0);
		lcd_string("Talanquera");
		lcd_set_cursor(1,0);
		lcd_string("cerrada");
		_delay_ms(800);
	}

	return autorizada; // 1 = válida, 0 = no válida
}


/*
static int leer_nivel_actual_placeholder(void){
    // TODO: integrar sensor real
    return 0;
}
*/

/*
static void enviar_cmd_motor_peso(char cmd){
    I2C_Master_Start();
    I2C_Master_Write((SLAVE_PESO<<1) | 0x00);
    I2C_Master_Write((uint8_t)cmd);
    I2C_Master_Stop();
}
*/


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


// ==================== IMPRIMIR PESO ====================
static void lcd_print_Peso(int peso) {
	char buf[6];
	snprintf(buf, sizeof(buf), "%03d", peso);   // siempre 3 dígitos
	lcd_set_cursor(0,0);
	lcd_string("P:");
	lcd_string(buf);
	lcd_string("g ");   // agrega espacios para limpiar basura
}

// ==================== IMPRIMIR DISTANCIA ====================
static void lcd_print_Distancia(int dist){
	char buf[6];
	if(dist > 999) dist = 999;                  // límite de 3 dígitos
	snprintf(buf, sizeof(buf), "%03d ", dist);
	lcd_set_cursor(0,7);
	lcd_string("D:");
	lcd_string(buf);
}

// ==================== IMPRIMIR ELEVADOR ====================
static void lcd_print_Elevador(int nivel){
	char buf[4];
	snprintf(buf, sizeof(buf), "%d", nivel);
	lcd_set_cursor(0,13);
	lcd_string("E:");
	lcd_string(buf);
}

// ==================== IMPRIMIR NFC ====================
static void lcd_print_NFC(const char *uid){
	lcd_set_cursor(1,0);
	lcd_string("NFC:");
	lcd_string(uid);   // imprime la contraseña/UID completa
}


// ========================
// MAIN
// ========================
int main(void){
	uint8_t peso_ent=0, peso_dec=0;
	uint8_t uid[UID_LEN] = {0};
	char uid_hex[UID_LEN*2 + 1];
	char uid_anterior[UID_LEN * 2 + 1] = "";
	static int last_distance = 0;
	uint8_t nfc_valida = 0;  // bandera NFC válida

	lcd_init();
	lcd_clear();
	lcd_set_cursor(0,0); lcd_string("Iniciando...");
	_delay_ms(800);

	UART_Init(MYUBRR);

	DDRC &= ~((1<<PC4)|(1<<PC5)|(1<<PC1));  // PC4-PC5 I2C, PC1 botón
	PORTC |= (1<<PC4)|(1<<PC5)|(1<<PC1);    // pull-ups I2C y botón
	I2C_Master_Init(50000, 1);

	if (VL53L0X_Init()) {
		UART_SendString("Sensor VL53L0X inicializado correctamente\r\n");
		} else {
		UART_SendString("ERROR: No se pudo inicializar el sensor VL53L0X\r\n");
		while(1);
	}

	lcd_clear();
	lcd_set_cursor(0,0); lcd_string("P:--g D:-- E:-");
	lcd_set_cursor(1,0); lcd_string("NFC:----------");

	// Variables previas para detectar cambios
	static int peso_prev = -1;
	static int dist_prev = -1;
	static char uid_prev[32] = "";

	for(;;){
		// ===== Lecturas normales =====
		leer_peso(&peso_ent, &peso_dec);
		lcd_print_Peso(peso_ent);

		VL53L0X_StartMeasurement();
		int raw = VL53L0X_ReadDistance() - 100;
		if (raw > 1000) raw = 0;
		if (abs(raw - last_distance) > 10) {
			last_distance = raw;
		}
		lcd_print_Distancia(last_distance);

		leer_uid_nfc(uid);
		bytes_to_hex(uid, UID_LEN, uid_hex);
		if (strcmp(uid_hex, uid_anterior) != 0) {
			// Hay un nuevo UID
			if (es_tarjeta_autorizada(uid_hex)) {
				nfc_valida = 1; // activamos bandera NFC válida
				} else {
				nfc_valida = 0;
			}
			strcpy(uid_anterior, uid_hex);
		}
		lcd_print_NFC(uid_hex);
		
		lcd_print_Elevador(elevadorNivel);

		// ==== Modo escucha UART con botón solo si NFC válida ====
		if (nfc_valida && !(PINC & (1<<PC1))) {  // NFC válida y botón presionado
			lcd_clear();
			lcd_set_cursor(0,0);
			lcd_string("Esperando valor...");
			UART_SendString("X/n")

			uint32_t start_time = 0;
			uint8_t recibido = 0;
			char uart_temp[UART_BUF_SIZE];
			uint8_t idx = 0;

			for (start_time = 0; start_time < 6000; start_time += 50) {
				_delay_ms(50);

				while (UART_Available()) {
					char c = UART_ReadChar();

					if (c == '\n') {
						uart_temp[idx] = '\0';
						idx = 0;

						if (strncmp(uart_temp, "E:", 2) == 0) {
							elevadorNivel = atoi(&uart_temp[2]);
							recibido = 1;
							break;
						}
						} else if (idx < UART_BUF_SIZE-1) {
						uart_temp[idx++] = c;
					}
				}

				if (recibido) break;
			}

			if (!recibido) {
				lcd_clear();
				lcd_set_cursor(0,0);
				lcd_string("Timeout 6s");
				_delay_ms(800);
			}

			// Salir del modo escucha y volver al loop normal
			lcd_clear();
			lcd_set_cursor(0,0); lcd_string("P:--g D:-- E:-");
			lcd_set_cursor(1,0); lcd_string("NFC:----------");

			// Reiniciar variables previas
			peso_prev = -1;
			dist_prev = -1;
			strcpy(uid_prev, "");
			nfc_valida = 0; // se desactiva la bandera
		}

		// ==== Envío de datos normales solo si botón NO presionado ====
		else {
			if (peso_ent != peso_prev) {
				char out[16];
				snprintf(out, sizeof(out), "P:%d\n", peso_ent);
				UART_SendString(out);
				peso_prev = peso_ent;
			}

			if (last_distance != dist_prev) {
				char out[12];
				snprintf(out, sizeof(out), "D:%d\n", last_distance);
				UART_SendString(out);
				dist_prev = last_distance;
			}

			if (strcmp(uid_hex, uid_prev) != 0) {
				UART_SendString("N:");
				UART_SendString(uid_hex);
				UART_SendString("\n");
				strcpy(uid_prev, uid_hex);
			}
		}

		_delay_ms(300);
	}
}
