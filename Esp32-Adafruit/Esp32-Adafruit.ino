/***************************************************
 * ESP32 + Adafruit IO + UART
 * Envía datos del Maestro (Peso, Distancia y NFC)
 * y recibe comandos (Talanquera y Elevador)
 * 
 * Si recibe "X" desde el Maestro → entra en modo
 * bloqueo 6 s, deja de escuchar Maestro y espera
 * un comando de elevador desde Adafruit.
 ***************************************************/

#include "WiFi.h"
#include "AdafruitIO_WiFi.h"

// ======== CONFIGURACIÓN WIFI ========
#define WIFI_SSID       ""
#define WIFI_PASS       ""

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

// Buffer para comando elevador pendiente
volatile bool elevadorPendiente = false;
String comandoElevador = "";

// --- Control de estabilidad para Peso ---
unsigned long lastChangePeso = 0;
const unsigned long stableDelay = 2000;  // tiempo para considerar estable (ms)

String pesoPendiente = "";
String ultimoPeso = "";

// Últimos valores enviados (para Distancia y NFC)
String ultimaDistancia = "";
String ultimaContrasena = "";

// --- Control de modo bloqueo ---
bool modoBloqueo = false;          
unsigned long inicioBloqueo = 0;
const unsigned long tiempoBloqueo = 6000;  // 6 segundos

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

  // --- Si estamos en modo bloqueo ---
  if (modoBloqueo) {
    // esperar elevador desde Adafruit (handleElevador ya lo capta)
    if (elevadorPendiente) {
      Serial2.println(comandoElevador);
      Serial.print("📤 Elevador enviado al Maestro: ");
      Serial.println(comandoElevador);
      elevadorPendiente = false;

      // salir del modo bloqueo
      modoBloqueo = false;
      Serial.println("✅ Salida de modo bloqueo");
    }

    // si pasan los 6 segundos y no llegó nada, liberar bloqueo
    if (millis() - inicioBloqueo > tiempoBloqueo) {
      modoBloqueo = false;
      Serial.println("⏱️ Tiempo agotado, regreso a modo normal");
    }

    return; // ignorar lectura del maestro mientras estamos bloqueados
  }

  // --- Lectura desde Maestro (modo normal) ---
  if (Serial2.available()) {
    String valor = Serial2.readStringUntil('\n');
    valor.trim();

    Serial.print("📥 Dato recibido del Maestro: ");
    Serial.println(valor);

    // --- Caso especial: activar modo bloqueo ---
    if (valor == "X") {
      modoBloqueo = true;
      inicioBloqueo = millis();
      Serial.println("🚫 Entrando en modo bloqueo (espera Adafruit)");
      return;  // no procesar más
    }

    // --- Peso ---
    if (valor.startsWith("P:")) {
      String peso = valor.substring(2);
      if (peso != ultimoPeso) {
        pesoPendiente = peso;
        lastChangePeso = millis();
      }
    }

    // --- Distancia ---
    else if (valor.startsWith("D:")) {
      String distancia = valor.substring(2);
      if (distancia != ultimaDistancia) {
        feed_distancia->save(distancia);
        Serial.print("📤 Distancia enviada a Adafruit: ");
        Serial.println(distancia);
        ultimaDistancia = distancia;
      }
    }

    // --- Contraseña (NFC) ---
    else if (valor.startsWith("N:")) {
      String contrasena = valor.substring(2);
      if (contrasena != ultimaContrasena) {
        feed_contrasena->save(contrasena);
        Serial.print("📤 Contraseña enviada a Adafruit: ");
        Serial.println(contrasena);
        ultimaContrasena = contrasena;
      }
    }
  }

  // --- Revisión de estabilidad de Peso ---
  if (pesoPendiente != "" && millis() - lastChangePeso > stableDelay) {
    feed_peso->save(pesoPendiente);
    Serial.print("📤 Peso estable enviado a Adafruit: ");
    Serial.println(pesoPendiente);

    ultimoPeso = pesoPendiente;
    pesoPendiente = "";
  }
}
