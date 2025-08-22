/***************************************************
 * ESP32 + Adafruit IO + UART
 * Envía datos del Maestro (Peso, Distancia y NFC)
 * y recibe comandos (Talanquera y Elevador)
 ***************************************************/

#include "WiFi.h"
#include "AdafruitIO_WiFi.h"

// ======== CONFIGURACIÓN WIFI ========
#define WIFI_SSID       "CRTSTDS"
#define WIFI_PASS       "ngmc6126"

// ======== CONFIGURACIÓN ADAFRUIT IO ========
#define IO_USERNAME     ""
#define IO_KEY          ""

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Feeds de salida
AdafruitIO_Feed *feed_peso       = io.feed("peso");
AdafruitIO_Feed *feed_distancia  = io.feed("distancia");
AdafruitIO_Feed *feed_contrasena = io.feed("contrasena");

// Feeds de entrada
AdafruitIO_Feed *feed_elevador   = io.feed("elevador");

// Prototipos de callbacks
void handleElevador(AdafruitIO_Data *data);

// Control de tiempo
unsigned long lastRequest     = 0;
const unsigned long reqPeriod = 2000;   // cada 2s pedimos datos al Maestro

// Control de envío a Adafruit
unsigned long lastSendPeso       = 0;
unsigned long lastSendDistancia  = 0;
unsigned long lastSendContrasena = 0;
const unsigned long sendInterval = 1500;

// Buffer para comando elevador pendiente
volatile bool elevadorPendiente = false;
String comandoElevador = "";

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // UART hacia Maestro
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17

  Serial.println("Conectando a Adafruit IO...");
  io.connect();

  // Suscripción al feed de elevador
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

  // Guardar comando pendiente
  comandoElevador = "E:" + String(valor);
  elevadorPendiente = true;
}

void loop() {
  io.run();

  // --- Enviar comando elevador si está pendiente ---
  if (elevadorPendiente) {
    Serial2.println(comandoElevador);
    Serial.print("📤 Elevador enviado al Maestro: ");
    Serial.println(comandoElevador);
    elevadorPendiente = false;   // se envió, limpiar bandera
  }

  // --- Solicitar datos periódicamente ---
  if (millis() - lastRequest > reqPeriod) {
    Serial2.println("P?");
    delay(30);
    Serial2.println("D?");
    delay(30);
    Serial2.println("N?");
    lastRequest = millis();
  }

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

    // --- Contraseña (NFC) ---
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
