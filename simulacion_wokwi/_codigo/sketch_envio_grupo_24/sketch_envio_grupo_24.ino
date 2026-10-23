/*
 * TPI IoT 4K3 - Grupo 24 - SKETCH 1: ENVIO
 * Se carga por USB. Sensa, muestra en el OLED, envia a ThingSpeak cada 16 s
 * y habilita OTA (ArduinoOTA) para subir el Sketch 2 por WiFi.
 *
 * Placa de la catedra (DOIT ESP32 DEVKIT V1):
 *   Potenciometro  GPIO32     DHT22 (T y H)  GPIO33
 *   LED integrado  GPIO2      LED externo    GPIO23 (verde)
 *   Touch +        GPIO13(T4) Touch -        GPIO4 (T0)
 *   OLED SH1106    SDA GPIO21 / SCL GPIO22 / direccion 0x3C
 *
 * Librerias: DHT sensor library, Adafruit Unified Sensor, Adafruit SH110X,
 *            Adafruit GFX Library, ThingSpeak. (WiFi y ArduinoOTA vienen con la ESP32)
 */
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== CONFIGURACION (completar) =====================
const char *WIFI_SSID = "Wokwi-GUEST";   // Red 2.4 GHz (el hotspot del celu sirve)
const char *WIFI_PASS = "";
unsigned long CANAL_ID = 3546746;                   // Channel ID de ThingSpeak
const char *WRITE_API_KEY = "K50CG6XHY1HWXQN8";
const char *OTA_HOSTNAME = "esp32-grupo24";   // Nombre del puerto de red en el IDE
// =====================================================================

// Pines
const int PIN_POTE = 32;
const int PIN_DHT = 33;
const int PIN_LED_INT = 2;
const int PIN_LED_EXT = 23;
const int PIN_TOUCH_MAS = T4;   // GPIO13
const int PIN_TOUCH_MENOS = T0; // GPIO4

// PWM de 12 bits -> valores de 0 a 4095
const int PWM_FREQ = 5000;
const int PWM_BITS = 12;
const int PWM_MAX = 4095;
const int CANAL_PWM_INT = 0;    // solo se usan con el core ESP32 2.x
const int CANAL_PWM_EXT = 1;

// Tiempos (ms)
const unsigned long INTERVALO_ENVIO = 16000;   // ThingSpeak gratis: minimo 15 s
const unsigned long INTERVALO_DHT = 2000;      // el DHT22 no lee mas rapido
const unsigned long INTERVALO_TOUCH = 50;
const unsigned long INTERVALO_DISPLAY = 200;

const int PASO_TOUCH = PWM_MAX / 100;          // 1% por paso mientras se toca

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(PIN_DHT, DHT22);
WiFiClient cliente;

int pwmLedInt = 0;          // 0..4095, sale del potenciometro
int pwmLedExt = 0;          // 0..4095, se mueve con los touch
float temperatura = NAN;
float humedad = NAN;
int umbralTouchMas = 0;
int umbralTouchMenos = 0;
int ultimoEstadoEnvio = 0;  // 200 = OK, 0 = todavia no se envio

unsigned long tEnvio = 0, tDht = 0, tTouch = 0, tDisplay = 0;

// ---------- PWM compatible con core ESP32 2.x y 3.x ----------
void pwmIniciar(int pin, int canal) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(canal, PWM_FREQ, PWM_BITS);
  ledcAttachPin(pin, canal);
#endif
}

void pwmEscribir(int pin, int canal, int valor) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, valor);
#else
  ledcWrite(canal, valor);
#endif
}

// ---------- Display ----------
void mostrarMensaje(const char *linea1, const char *linea2) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(linea1);
  display.println(linea2);
  display.display();
}

void actualizarDisplay() {
  int porcentajeExt = pwmLedExt * 100 / PWM_MAX;
  long faltan = (long)(INTERVALO_ENVIO - (millis() - tEnvio)) / 1000;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("TPI G24 - ENVIO");
  display.printf("PWM LED int: %d\n", pwmLedInt);
  display.printf("LED ext: %d %%\n", porcentajeExt);
  display.printf("Temp: %.1f C\n", temperatura);
  display.printf("Hum:  %.1f %%\n", humedad);
  display.printf("Envio en %lds ", faltan < 0 ? 0 : faltan);
  display.println(ultimoEstadoEnvio == 200 ? "[OK]" : (ultimoEstadoEnvio == 0 ? "[--]" : "[ERR]"));
  display.print("IP ");
  display.println(WiFi.localIP());
  display.display();
}

// ---------- WiFi y OTA ----------
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
    // Apagamos los LEDs y avisamos en pantalla mientras se sube el Sketch 2
    pwmEscribir(PIN_LED_INT, CANAL_PWM_INT, 0);
    pwmEscribir(PIN_LED_EXT, CANAL_PWM_EXT, 0);
    mostrarMensaje("Actualizando OTA...", "No apagar la placa");
  });
  ArduinoOTA.onProgress([](unsigned int progreso, unsigned int total) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Actualizando OTA...");
    display.printf("%u %%", progreso * 100 / total);
    display.display();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error OTA [%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.printf("OTA listo: puerto de red '%s'\n", OTA_HOSTNAME);
}

// ---------- Touch ----------
// touchRead() BAJA cuando se toca el pin. Tomamos el valor en reposo
// al arrancar y consideramos "tocado" por debajo del 60% de ese valor.
int calibrarTouch(int pin) {
  long suma = 0;
  for (int i = 0; i < 20; i++) {
    suma += touchRead(pin);
    delay(10);
  }
  return (suma / 20) * 60 / 100;
}

void leerTouch() {
  if (touchRead(PIN_TOUCH_MAS) < umbralTouchMas) {
    pwmLedExt = min(pwmLedExt + PASO_TOUCH, PWM_MAX);
  }
  if (touchRead(PIN_TOUCH_MENOS) < umbralTouchMenos) {
    pwmLedExt = max(pwmLedExt - PASO_TOUCH, 0);
  }
  pwmEscribir(PIN_LED_EXT, CANAL_PWM_EXT, pwmLedExt);
}

// ---------- Sensores ----------
void leerPotenciometro() {
  pwmLedInt = analogRead(PIN_POTE);   // ADC de 12 bits: 0..4095, igual que el PWM
  pwmEscribir(PIN_LED_INT, CANAL_PWM_INT, pwmLedInt);
}

void leerDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {   // si la lectura falla, conservamos la anterior
    temperatura = t;
    humedad = h;
  }
}

// ---------- ThingSpeak ----------
void enviarThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }
  int aleatorio = random(100, 501);   // 100..500
  long segundos = millis() / 1000;
  int porcentajeExt = pwmLedExt * 100 / PWM_MAX;

  // Los 6 campos van en UNA sola escritura (limite de 15 s de ThingSpeak)
  ThingSpeak.setField(1, aleatorio);
  ThingSpeak.setField(2, temperatura);
  ThingSpeak.setField(3, humedad);
  ThingSpeak.setField(4, segundos);
  ThingSpeak.setField(5, pwmLedInt);
  ThingSpeak.setField(6, porcentajeExt);
  ultimoEstadoEnvio = ThingSpeak.writeFields(CANAL_ID, WRITE_API_KEY);

  Serial.printf("Envio -> rnd=%d T=%.1f H=%.1f t=%lds PWM=%d ext=%d%% | ",
                aleatorio, temperatura, humedad, segundos, pwmLedInt, porcentajeExt);
  if (ultimoEstadoEnvio == 200) {
    Serial.println("OK");
  } else {
    Serial.printf("ERROR HTTP %d\n", ultimoEstadoEnvio);
  }
}

void setup() {
  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  analogReadResolution(12);
  pwmIniciar(PIN_LED_INT, CANAL_PWM_INT);
  pwmIniciar(PIN_LED_EXT, CANAL_PWM_EXT);
  dht.begin();

  // No tocar los pines touch durante el arranque (se calibran ahora)
  umbralTouchMas = calibrarTouch(PIN_TOUCH_MAS);
  umbralTouchMenos = calibrarTouch(PIN_TOUCH_MENOS);
  Serial.printf("Umbrales touch: +%d  -%d\n", umbralTouchMas, umbralTouchMenos);

  randomSeed(esp_random());
  conectarWiFi();
  iniciarOTA();
  ThingSpeak.begin(cliente);

  leerDHT();
  tEnvio = millis();
}

void loop() {
  ArduinoOTA.handle();   // siempre primero: atiende la subida del Sketch 2
  unsigned long ahora = millis();

  leerPotenciometro();

  if (ahora - tTouch >= INTERVALO_TOUCH) {
    tTouch = ahora;
    leerTouch();
  }
  if (ahora - tDht >= INTERVALO_DHT) {
    tDht = ahora;
    leerDHT();
  }
  if (ahora - tDisplay >= INTERVALO_DISPLAY) {
    tDisplay = ahora;
    actualizarDisplay();
  }
  if (ahora - tEnvio >= INTERVALO_ENVIO) {
    tEnvio = ahora;
    enviarThingSpeak();
  }
}
