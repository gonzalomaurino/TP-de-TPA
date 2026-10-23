/*
 * TPI IoT 4K3 - Grupo 24 - SKETCH 2: LECTURA
 *
 * Integrantes (Apellido, Nombre - Legajo):
 *   Zengaro, Tomas - 403344
 *   Visintin, Juan - 95843
 *   Bongiorno, Valentino - 98495
 *   Peralta, Santino - 403430
 *   Carrizo, Martin - 400562
 *   Maurino, Gonzalo - 401062
 *
 * Se sube por OTA desde el puerto de red que habilito el Sketch 1.
 * Lee los 6 campos del canal de ThingSpeak, cuenta las pulsaciones del
 * pulsador y muestra todo en el OLED en 2 pantallas rotativas de 3 campos.
 * Tambien mantiene OTA activo para poder volver a subir por WiFi.
 *
 * Placa de la catedra: Pulsador GPIO19 (push del encoder), OLED SH1106 0x3C
 */
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ===================== CONFIGURACION (completar) =====================
const char *WIFI_SSID = "CIDS";   // La misma red que el Sketch 1
const char *WIFI_PASS = "NuevaClave2024";
unsigned long CANAL_ID = 3546746;                   // El MISMO canal (publico) del Sketch 1
const char *OTA_HOSTNAME = "esp32-grupo24";
// =====================================================================

const int PIN_PULSADOR = 19;

const unsigned long INTERVALO_LECTURA = 16000;  // mismo ritmo con el que se escribe
const unsigned long INTERVALO_PANTALLA = 4000;  // cambia de pantalla cada 4 s
const unsigned long INTERVALO_DISPLAY = 200;
const unsigned long ANTIRREBOTE_MS = 200;

const char *NOMBRES[6] = {
  "Aleatorio", "Temp (C)", "Humedad %", "Tiempo (s)", "PWM LED int", "LED ext %"
};

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
WiFiClient cliente;

float campos[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
int ultimoEstadoLectura = 0;   // 200 = OK
int pantalla = 0;              // 0 = campos 1-3, 1 = campos 4-6

// El pulsador se cuenta por interrupcion: asi no se pierden pulsaciones
// mientras la ESP32 esta esperando la respuesta de ThingSpeak.
volatile unsigned long pulsaciones = 0;
volatile unsigned long tUltimaPulsacion = 0;

unsigned long tLectura = 0, tPantalla = 0, tDisplay = 0, tUltimaLecturaOk = 0;

void IRAM_ATTR alPresionar() {
  unsigned long ahora = millis();
  if (ahora - tUltimaPulsacion > ANTIRREBOTE_MS) {
    pulsaciones = pulsaciones + 1;
    tUltimaPulsacion = ahora;
  }
}

void mostrarMensaje(const char *linea1, const char *linea2) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(linea1);
  display.println(linea2);
  display.display();
}

void actualizarDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("TPI G24 - LECTURA %d/2\n", pantalla + 1);
  display.println("---------------------");
  for (int i = pantalla * 3; i < pantalla * 3 + 3; i++) {
    if (isnan(campos[i])) {
      display.printf("F%d %-11s --\n", i + 1, NOMBRES[i]);
    } else if (i == 1 || i == 2) {
      display.printf("F%d %-11s %.1f\n", i + 1, NOMBRES[i], campos[i]);
    } else {
      display.printf("F%d %-11s %.0f\n", i + 1, NOMBRES[i], campos[i]);
    }
  }
  display.println("---------------------");
  display.printf("Pulsaciones: %lu\n", pulsaciones);
  if (ultimoEstadoLectura == 200) {
    display.printf("Leido hace %lus", (millis() - tUltimaLecturaOk) / 1000);
  } else if (ultimoEstadoLectura == 0) {
    display.print("Esperando datos...");
  } else {
    display.printf("Error lectura %d", ultimoEstadoLectura);
  }
  display.display();
}

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mostrarMensaje("Conectando WiFi...", WIFI_SSID);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConectado. IP: ");
  Serial.println(WiFi.localIP());
}

void iniciarOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    mostrarMensaje("Actualizando OTA...", "No apagar la placa");
  });
  ArduinoOTA.begin();
}

void leerThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }
  // Canal publico: no hace falta Read API Key. Trae el ultimo registro completo.
  ultimoEstadoLectura = ThingSpeak.readMultipleFields(CANAL_ID);
  if (ultimoEstadoLectura == 200) {
    for (int i = 0; i < 6; i++) {
      campos[i] = ThingSpeak.getFieldAsFloat(i + 1);
    }
    tUltimaLecturaOk = millis();
    Serial.printf("Lectura OK: %.0f | %.1f | %.1f | %.0f | %.0f | %.0f\n",
                  campos[0], campos[1], campos[2], campos[3], campos[4], campos[5]);
  } else {
    Serial.printf("Error leyendo el canal. HTTP %d\n", ultimoEstadoLectura);
  }
}

void setup() {
  Serial.begin(115200);

  display.begin(0x3C, true);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  pinMode(PIN_PULSADOR, INPUT_PULLUP);   // suelto = HIGH, apretado = LOW
  attachInterrupt(digitalPinToInterrupt(PIN_PULSADOR), alPresionar, FALLING);

  conectarWiFi();
  iniciarOTA();
  ThingSpeak.begin(cliente);

  leerThingSpeak();   // primera lectura al arrancar
  tLectura = millis();
}

void loop() {
  ArduinoOTA.handle();
  unsigned long ahora = millis();

  if (ahora - tLectura >= INTERVALO_LECTURA) {
    tLectura = ahora;
    leerThingSpeak();
  }
  if (ahora - tPantalla >= INTERVALO_PANTALLA) {
    tPantalla = ahora;
    pantalla = (pantalla + 1) % 2;
  }
  if (ahora - tDisplay >= INTERVALO_DISPLAY) {
    tDisplay = ahora;
    actualizarDisplay();
  }
}
