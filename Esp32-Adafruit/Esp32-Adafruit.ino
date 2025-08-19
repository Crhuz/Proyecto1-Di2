/***************************************************
 * ESP32 + Adafruit IO + UART
 * Envía datos del Maestro (Peso, Distancia y NFC)
 * y recibe comandos (Talanquera y Elevador)
 ***************************************************/

#include "WiFi.h"
#include "AdafruitIO_WiFi.h"

// ======== CONFIGURACIÓN WIFI ========
#define WIFI_SSID       "ARRIS-EAC2"
#define WIFI_PASS       "2PM7H7601150"

// ======== CONFIGURACIÓN ADAFRUIT IO ========
#define IO_USERNAME     "Chruz"
#define IO_KEY          "aio_HDyF04UNUcMJ4KLCUAZsxuR8pNYx"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Feeds de salida
AdafruitIO_Feed *feed_peso       = io.feed("peso");
AdafruitIO_Feed *feed_distancia  = io.feed("distancia");
AdafruitIO_Feed *feed_contrasena = io.feed("contrasena");   // <<-- nuevo feed

// Feeds de entrada
AdafruitIO_Feed *feed_elevador   = io.feed("elevador");

// Prototipos de callbacks
void handleElevador(AdafruitIO_Data *data);

// Control de tiempo independiente
unsigned long lastSendPeso       = 0;
unsigned long lastSendDistancia  = 0;
unsigned long lastSendContrasena = 0;
const unsigned long sendInterval = 1000; // 1s entre envíos para evitar saturación

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // UART hacia Maestro
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17

  Serial.println("Conectando a Adafruit IO...");
  io.connect();

  // Suscripciones a feeds de entrada
  feed_elevador->onMessage(handleElevador);

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Conectado a Adafruit IO");
}


// --- Callback elevador ---
void handleElevador(AdafruitIO_Data *data) {
  int valor = data->toInt();
  Serial.print("📥 Elevador desde Adafruit: ");
  Serial.println(valor);

  // Mandar al Maestro
  Serial2.print("E:");
  Serial2.println(valor);
}

void loop() {
  io.run();

  // --- Lectura desde Maestro ---
  if (Serial2.available()) {
    String valor = Serial2.readStringUntil('\n');
    valor.trim();

    Serial.print("📥 Dato recibido del Maestro: ");
    Serial.println(valor);

    // --- Peso ---
    if (valor.startsWith("P:")) {
      if (millis() - lastSendPeso > sendInterval) {
        String peso = valor.substring(2);
        feed_peso->save(peso);
        Serial.print("📤 Peso enviado a Adafruit: ");
        Serial.println(peso);
        lastSendPeso = millis();
      }
    }

    // --- Distancia ---
    else if (valor.startsWith("D:")) {
      if (millis() - lastSendDistancia > sendInterval) {
        String distancia = valor.substring(2);
        feed_distancia->save(distancia);
        Serial.print("📤 Distancia enviada a Adafruit: ");
        Serial.println(distancia);
        lastSendDistancia = millis();
      }
    }

    // --- Contrasena (NFC/Primer byte) ---
    else if (valor.startsWith("N:")) {
      if (millis() - lastSendContrasena > sendInterval) {
        String contrasena = valor.substring(2);
        feed_contrasena->save(contrasena);
        Serial.print("📤 Contraseña enviada a Adafruit: ");
        Serial.println(contrasena);
        lastSendContrasena = millis();
      }
    }
  }
}
