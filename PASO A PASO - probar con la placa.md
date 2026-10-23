# Paso a paso: probar el TPI con la placa de la cátedra

Tiempo estimado: 30–45 min la primera vez. Hagan los pasos A y B **antes** de ir a buscar la placa.

---

## A. Preparar la notebook (una sola vez, en casa)

1. Instalar **Arduino IDE 2**: https://www.arduino.cc/en/software
2. En Arduino IDE ir a *Archivo → Preferencias → "URLs adicionales del gestor de placas"* y pegar:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. *Herramientas → Placa → Gestor de placas*: buscar **esp32** (de Espressif Systems) → Instalar. Pesa unos 2 GB.
4. *Herramientas → Gestionar bibliotecas*: instalar estas cinco (si pregunta por dependencias, aceptar "Instalar todas"):
   - **DHT sensor library** (Adafruit)
   - **Adafruit Unified Sensor**
   - **Adafruit SH110X**
   - **Adafruit GFX Library**
   - **ThingSpeak** (MathWorks)
5. Driver del puerto USB: si al enchufar la placa no aparece ningún puerto COM, instalar el que está en `tpa/01 - Recursos - General/instalador puerto com.zip`.

## B. Crear el canal de ThingSpeak (una sola vez)

1. Entrar a https://thingspeak.mathworks.com con el **mail institucional** (hay un tutorial en `tpa/01 - Recursos - General/Tutorial Thingspeak.pdf`).
2. *Channels → New Channel*: nombre "TPI Grupo 24". Activar **6 fields** con estos nombres:
   1. Aleatorio · 2. Temperatura · 3. Humedad · 4. Tiempo (s) · 5. PWM LED int · 6. LED ext %
3. *Save Channel*.
4. Pestaña **Sharing** → "Share channel view with everyone". **El canal tiene que ser público**, porque el Sketch 2 lo lee sin clave.
5. En la pestaña **API Keys**, copiar el **Channel ID** y la **Write API Key**.
6. Completar esos dos datos en:
   - `sketch_envio_grupo_24.ino` → `CANAL_ID` y `WRITE_API_KEY`
   - `sketch_lectura_grupo_24.ino` → `CANAL_ID`
   - `integrantes_grupo_24.txt`

## C. Elegir la red WiFi (importante)

- **Usen el hotspot de un celular.** Las redes de la facu suelen bloquear que la PC "vea" a la ESP32, y sin eso el OTA no funciona.
- El hotspot tiene que estar en **2.4 GHz**, porque la ESP32 no se conecta a redes de 5 GHz. En el iPhone: activar "Maximizar compatibilidad". En Android: Banda AP → 2.4 GHz.
- **La notebook también se conecta a ese hotspot.**
- Completar `WIFI_SSID` y `WIFI_PASS` en **los dos** sketches.

## D. Cargar el Sketch 1 por USB

1. Enchufar la placa por USB.
2. *Herramientas → Placa → esp32 → **DOIT ESP32 DEVKIT V1***.
3. *Herramientas → Puerto → COMx* (el que aparece al enchufar).
4. Abrir `sketch_envio_grupo_24/sketch_envio_grupo_24.ino` → botón **Subir** (la flecha).
   - Si se queda en `Connecting.....`, mantener apretado el botón **BOOT** de la ESP32 hasta que empiece a subir.
5. **No toquen los pines touch durante los primeros 2 segundos**, porque la placa los está calibrando.
6. Abrir el *Monitor Serie* a **115200** baudios. Tienen que ver la IP y, cada 16 s, una línea `Envio -> ... OK`.

**Qué tienen que ver en la placa:**
- El potenciómetro cambia el brillo del LED azul de la ESP32, y en pantalla se mueve `PWM LED int` (0–4095).
- Tocando GPIO13 sube el LED verde y tocando GPIO4 baja. En pantalla se ve el `%`.
- La temperatura y la humedad aparecen en pantalla.
- Cada 16 s aparece `[OK]` en pantalla y los gráficos de ThingSpeak suman un punto.

## E. Subir el Sketch 2 por OTA (sin cable de datos)

1. Dejar la placa funcionando con el Sketch 1. Pueden desenchufar el USB y alimentarla con un cargador o un power bank: eso **demuestra** que la carga es por WiFi.
2. Cerrar y volver a abrir Arduino IDE si hace falta, y esperar unos 30 s.
3. En *Herramientas → Puerto* tiene que aparecer un **puerto de red**: `esp32-grupo24 at 192.168.x.x`. Elegirlo.
4. Abrir `sketch_lectura_grupo_24/sketch_lectura_grupo_24.ino` → **Subir**.
   - Si Windows pregunta por el **Firewall**, darle "Permitir".
   - En la pantalla de la placa se ve "Actualizando OTA... %".
5. La placa se reinicia con el Sketch 2.

**Qué tienen que ver:**
- Dos pantallas que se alternan cada 4 s: F1–F3 y F4–F6 con los valores del canal.
- `Pulsaciones: N` sube cada vez que aprietan el pulsador (el botón del encoder).
- `Leido hace Xs`: se vuelve a leer cada 16 s.

## Si algo falla

| Problema | Solución |
|---|---|
| Pantalla negra | Revisar que la placa esté bien enchufada. Las líneas del OLED son SDA=21 y SCL=22, dirección 0x3C |
| Se queda en "Conectando WiFi..." | El SSID o la clave están mal, o la red es de 5 GHz |
| `[ERR]` / error HTTP 400 o 401 | La Write API Key o el Channel ID están mal |
| Error HTTP -401 o envíos que fallan cada tanto | Se está escribiendo más rápido que cada 15 s, por ejemplo si dos placas usan el mismo canal |
| Temp/Hum en `nan` | El DHT tarda unos segundos en dar la primera lectura. Si sigue así, revisar el pin 33 |
| El touch no responde o se mueve solo | Reiniciar la placa (botón EN) **sin tocar** los pines, para que recalibre |
| No aparece el puerto de red | La PC y la ESP32 no están en la misma red, o el firewall lo bloquea. Probar con el hotspot del celu |
| El Sketch 2 muestra `--` | El canal no es público, o todavía no tiene datos. Dejen correr el Sketch 1 al menos 1 minuto antes |
| El pote funciona al revés | Es solo el sentido de giro, no es un error. Si quieren invertirlo: `pwmLedInt = PWM_MAX - analogRead(PIN_POTE);` |

## Entrega (miércoles 28/10, hasta las 23:59)

Un solo integrante sube a la tarea de la UV:
- `sketch_envio_grupo_24.ino`
- `sketch_lectura_grupo_24.ino`
- `integrantes_grupo_24.txt` (con el Channel ID y la Write API Key ya completados)
