/*
 * Esclavo_Peso_Final_fix.c
 * Responde 2 bytes binarios: [entero][decimas*100]
 * Ahora también recibe NFC desde el maestro ('N' + valor) y levanta la talanquera
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

// ---------- UART debug -----------
static void UART_Init(void){
    UBRR0 = 103;                 // 9600 @16MHz
    UCSR0B = (1<<TXEN0);
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}
static void UART_Tx(char c){ while(!(UCSR0A&(1<<UDRE0))); UDR0 = c; }
static void UART_Print(const char*s){ while(*s) UART_Tx(*s++); }
// ---------------------------------

// HX711
#define HX711_DT_PIN  PB1
#define HX711_SCK_PIN PB0
#define SLAVE_ADDR    0x20

// SERVO (Talanquera)
#define SERVO_PIN     PD6   // OC0A (Timer0 PWM)

#define NUM_LECT_TARA 20
#define FACTOR_CAL    1065.0f

volatile uint8_t cmd_byte = 0;
volatile uint8_t parte_entera = 0;
volatile uint8_t parte_decimal = 0;
volatile uint8_t ultimo_nfc = 0;   // último NFC recibido

// -------- Servo funciones ----------
static void Servo_Init(void){
    DDRD |= (1<<SERVO_PIN);
    // Timer0 Fast PWM, modo no-invertido
    TCCR0A = (1<<COM0A1)|(1<<WGM01)|(1<<WGM00);
    TCCR0B = (1<<WGM02)|(1<<CS01)|(1<<CS00);  // clk/64
    OCR0A = 0; // duty inicial
}

// genera pulso ~1ms (0°) a ~2ms (90°)
static void Servo_Angle(uint8_t ang){
    // map ang 0-180 a OCR0A
    uint16_t min = 31;   // ~1ms
    uint16_t max = 62;   // ~2ms
    OCR0A = min + ((max-min)*ang)/180;
}

static inline long HX711_Read(void){
    while(PINB & (1<<HX711_DT_PIN));
    long c = 0;
    for(uint8_t i=0;i<24;i++){
        PORTB |=  (1<<HX711_SCK_PIN);
        _delay_us(1);
        c <<= 1;
        PORTB &= ~(1<<HX711_SCK_PIN);
        _delay_us(1);
        if(PINB & (1<<HX711_DT_PIN)) c++;
    }
    PORTB |=  (1<<HX711_SCK_PIN); _delay_us(1);
    PORTB &= ~(1<<HX711_SCK_PIN);
    if(c & 0x800000) c |= 0xFF000000;
    return c;
}

static long offset_tara = 0;
static void hacer_tara(void){
    UART_Print("Tara...\r\n");
    offset_tara = 0;
    for(uint8_t i=0;i<NUM_LECT_TARA;i++){ offset_tara += HX711_Read(); _delay_ms(40); }
    offset_tara /= NUM_LECT_TARA;
    UART_Print("OK\r\n");
}

// ---- I2C esclavo básico ----
static void I2C_Slave_Init(uint8_t addr){
    TWAR = (addr<<1);                            
    TWCR = (1<<TWEA)|(1<<TWEN)|(1<<TWIE)|(1<<TWINT);
}
// -----------------------------------------------------

int main(void){
    // Pines HX711
    DDRB &= ~(1<<HX711_DT_PIN);
    DDRB |=  (1<<HX711_SCK_PIN);
    PORTB &= ~(1<<HX711_SCK_PIN);

    UART_Init();
    Servo_Init();
    UART_Print("Esclavo Peso listo\r\n");

    hacer_tara();

    I2C_Slave_Init(SLAVE_ADDR);
    sei();

    while(1){
        // Medición continua
        long lectura = HX711_Read();
        float peso = (lectura - offset_tara)/FACTOR_CAL;
        if(peso < 0) peso = 0;                         

        uint8_t entero = (uint8_t)peso;
        uint8_t dec    = (uint8_t)((peso - entero)*100.0f + 0.5f);

        parte_entera  = entero;
        parte_decimal = dec;

        char s[12];
        dtostrf(peso, 5, 2, s);
        UART_Print("Peso: "); UART_Print(s); UART_Print(" g\r\n");

        _delay_ms(400);
    }
}

// ---------- ISR TWI esclavo ----------
ISR(TWI_vect){
    static uint8_t idx = 0;
    static uint8_t esperando_nfc = 0;

    switch(TWSR & 0xF8){
        case 0x60: case 0x70: // SLA+W
            idx = 0;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0x80: case 0x90: // Data received
            if(idx == 0){
                cmd_byte = TWDR;
                if(cmd_byte == 'N'){
                    esperando_nfc = 1;  // próximo byte es NFC
                }
            } else {
                if(esperando_nfc){
                    ultimo_nfc = TWDR;
                    esperando_nfc = 0;
                    // Validación placeholder (acepta todos)
                    UART_Print("NFC recibido: ");
                    UART_Tx(ultimo_nfc);
                    UART_Print("\r\n");

                    // Levantar talanquera
                    Servo_Angle(180);      // abrir
                    _delay_ms(3000);       // 3s abierta
                    Servo_Angle(90);       // cerrar
                }
            }
            idx++;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0xA8: case 0xB8: // SLA+R
            TWDR = (idx==0) ? parte_entera : parte_decimal;
            idx = (idx+1) & 0x01;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0xC0: case 0xC8:
            idx = 0;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        default:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;
    }
}
