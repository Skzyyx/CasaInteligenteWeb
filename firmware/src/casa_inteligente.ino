// =============================================================
//  Casa Inteligente - Firmware ESP32
// =============================================================
//  Funciones:
//   - Lectura de sensores (DHT22, MQ-2, PIR)
//   - Lector RFID RC522 para control de acceso
//   - Activacion de actuadores (LED, buzzer, servomotor de cerradura)
//   - Servidor web sirviendo la UI desde LittleFS
//   - WebSocket para comunicacion en tiempo real con el dashboard
//   - Persistencia de alertas en MySQL externo
// =============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <MySQL_Generic.h>
#include "config.h"

// Defaults por si config.h no los define
#ifndef MODO_SIMULADOR
#define MODO_SIMULADOR 0
#endif
#ifndef INTERVALO_SIMULADOR_MS
#define INTERVALO_SIMULADOR_MS 8000
#endif

// ===== PINES =====
// Validados con los sketches de prueba individuales (test_mq6, test_pir, etc.)
#define PIN_DHT        4   // DHT11 (alimentar a 3.3V)
#define PIN_MQ2       34   // MQ-6 D0 (alimentar a 5V/VIN)
#define PIN_PIR       27   // HC-SR501 OUT (alimentar a 5V/VIN)
#define PIN_LED        2
#define PIN_BUZZER    15
#define PIN_BOTON     14
#define PIN_SERVO     13   // Movido de 27 a 13 porque el PIR ocupa el 27
#define PIN_RC522_SS   5
#define PIN_RC522_RST 22
// SPI por defecto del ESP32: SCK=18, MISO=19, MOSI=23

// Cambia entre DHT11 (azul) y DHT22/AM2302 (blanco) segun tu sensor
#define DHT_TIPO   DHT11

// ===== UMBRALES =====
#define TEMP_ALTA          30.0    // grados Celsius
#define TEMP_CRITICA       38.0
#define HUMEDAD_ALTA       65.0    // porcentaje
#define DEBOUNCE_MS        300

// Intervalo entre lecturas de sensores
// (DHT22 necesita >= 2000 ms, no bajar de ahi)
#define INTERVALO_LECTURA  2000    // ms
// Cooldown entre alertas del mismo sensor para no saturar la BD
#define COOLDOWN_ALERTA    3000    // ms
// Cada cuanto imprimir lecturas crudas en el monitor serie (debug)
#define INTERVALO_DEBUG    5000    // ms
// Tiempo que la cerradura permanece abierta tras un acceso autorizado
#define TIEMPO_PUERTA_MS   5000
// Posiciones del servo de la cerradura
#define SERVO_CERRADO      0
#define SERVO_ABIERTO      90

// ===== OBJETOS GLOBALES =====
DHT dht(PIN_DHT, DHT_TIPO);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiClient clienteSql;
MySQL_Connection conn((Client *)&clienteSql);
IPAddress ipServidorSql;

MFRC522 rfid(PIN_RC522_SS, PIN_RC522_RST);
Servo cerradura;

// ===== ESTADO =====
volatile bool alarmaActiva = false;
unsigned long ultimaLectura = 0;
unsigned long ultimaAlerta[4] = { 0, 0, 0, 0 }; // DHT22, MQ2, RFID, PIR
volatile unsigned long ultimoBoton = 0;
unsigned long puertaAbiertaDesde = 0;
unsigned long ultimoDebug = 0;

// Estado anterior de los sensores digitales para detectar flancos
int pirAnterior = LOW;

// =============================================================
//  UTILIDADES
// =============================================================
String escapar(const String &s) {
    String r = s;
    r.replace("'", "''");
    return r;
}

void controlActuadores() {
    digitalWrite(PIN_LED, alarmaActiva ? HIGH : LOW);
    digitalWrite(PIN_BUZZER, alarmaActiva ? HIGH : LOW);
}

void emitirEstadoAlarma() {
    StaticJsonDocument<128> doc;
    doc["tipo"] = "estado-alarma";
    JsonObject payload = doc.createNestedObject("payload");
    payload["alarma_activa"] = alarmaActiva;
    String salida;
    serializeJson(doc, salida);
    ws.textAll(salida);
}

// =============================================================
//  CONEXION A MySQL
// =============================================================
bool conectarSql() {
    if (conn.connected()) return true;

    Serial.println("Conectando a MySQL...");
    if (conn.connect(ipServidorSql, MYSQL_PORT, MYSQL_USER, MYSQL_PASS, MYSQL_DB)) {
        Serial.println("✅ Conectado a MySQL");
        return true;
    }
    Serial.println("❌ Falla al conectar a MySQL");
    return false;
}

void asegurarSql() {
    if (!conn.connected()) {
        conectarSql();
    }
}

// =============================================================
//  PERSISTENCIA DE ALERTAS
// =============================================================
bool guardarAlerta(const String &sensor, const String &tipo, const String &mensaje,
                   const String &valor, const String &severidad, long &idGenerado, String &fechaOut,
                   bool marcarAlarma = true) {
    asegurarSql();
    if (!conn.connected()) return false;

    char query[512];
    snprintf(query, sizeof(query),
        "INSERT INTO alertas (sensor, tipo, mensaje, valor, severidad) VALUES ('%s','%s','%s','%s','%s')",
        escapar(sensor).c_str(), escapar(tipo).c_str(), escapar(mensaje).c_str(),
        escapar(valor).c_str(), escapar(severidad).c_str());

    MySQL_Query consulta = MySQL_Query(&conn);
    if (!consulta.execute(query)) {
        Serial.println("❌ INSERT alerta fallo");
        return false;
    }

    if (marcarAlarma) {
        MySQL_Query updEstado = MySQL_Query(&conn);
        updEstado.execute("UPDATE estado_sistema SET alarma_activa = TRUE WHERE id = 1");
    }

    // Recuperar el id y fecha que MySQL acaba de generar
    MySQL_Query sel = MySQL_Query(&conn);
    if (sel.execute("SELECT LAST_INSERT_ID(), DATE_FORMAT(NOW(), '%Y-%m-%d %H:%i:%s')")) {
        column_names *cols = sel.get_columns();
        (void)cols;
        row_values *fila = sel.get_next_row();
        if (fila != NULL) {
            idGenerado = atol(fila->values[0]);
            fechaOut = String(fila->values[1]);
        }
    }
    return true;
}

void cargarHistorial(JsonArray &arr) {
    asegurarSql();
    if (!conn.connected()) return;

    const char *q = "SELECT id, sensor, tipo, mensaje, valor, severidad, "
                    "DATE_FORMAT(fecha, '%Y-%m-%d %H:%i:%s') AS f "
                    "FROM alertas ORDER BY fecha DESC LIMIT 50";

    MySQL_Query consulta = MySQL_Query(&conn);
    if (!consulta.execute(q)) return;

    column_names *cols = consulta.get_columns();
    (void)cols;
    row_values *fila;
    while ((fila = consulta.get_next_row()) != NULL) {
        JsonObject o = arr.createNestedObject();
        o["id"] = atol(fila->values[0]);
        o["sensor"] = fila->values[1] ? fila->values[1] : "";
        o["tipo"] = fila->values[2] ? fila->values[2] : "";
        o["mensaje"] = fila->values[3] ? fila->values[3] : "";
        o["valor"] = fila->values[4] ? fila->values[4] : "";
        o["severidad"] = fila->values[5] ? fila->values[5] : "media";
        o["fecha"] = fila->values[6] ? fila->values[6] : "";
    }
}

bool cargarEstadoAlarmaDesdeSql() {
    asegurarSql();
    if (!conn.connected()) return alarmaActiva;

    MySQL_Query consulta = MySQL_Query(&conn);
    if (!consulta.execute("SELECT alarma_activa FROM estado_sistema WHERE id = 1")) return alarmaActiva;

    column_names *cols = consulta.get_columns();
    (void)cols;
    row_values *fila = consulta.get_next_row();
    if (fila != NULL && fila->values[0] != NULL) {
        alarmaActiva = (atoi(fila->values[0]) != 0);
    }
    return alarmaActiva;
}

void apagarAlarmaPersistente() {
    asegurarSql();
    if (conn.connected()) {
        MySQL_Query upd = MySQL_Query(&conn);
        upd.execute("UPDATE estado_sistema SET alarma_activa = FALSE WHERE id = 1");
    }
    alarmaActiva = false;
    controlActuadores();
    emitirEstadoAlarma();
    Serial.println("🔕 Alarma apagada");
}

// =============================================================
//  EMITIR NUEVA ALERTA POR WEBSOCKET
// =============================================================
void disparar(const String &sensor, const String &tipo, const String &mensaje,
              const String &valor, const String &severidad, int idxCooldown,
              bool activarAlarma = true) {
    unsigned long ahora = millis();
#if !MODO_SIMULADOR
    if (ahora - ultimaAlerta[idxCooldown] < COOLDOWN_ALERTA) return;
#endif
    ultimaAlerta[idxCooldown] = ahora;

    long id = 0;
    String fecha = "";
    bool ok = guardarAlerta(sensor, tipo, mensaje, valor, severidad, id, fecha, activarAlarma);

    Serial.printf("🚨 [%s] %s (%s)\n", sensor.c_str(), mensaje.c_str(), valor.c_str());

    if (activarAlarma) {
        alarmaActiva = true;
        controlActuadores();
    }

    StaticJsonDocument<512> doc;
    doc["tipo"] = "nueva-alerta";
    JsonObject p = doc.createNestedObject("payload");
    p["id"] = id;
    p["sensor"] = sensor;
    p["tipo"] = tipo;
    p["mensaje"] = mensaje;
    p["valor"] = valor;
    p["severidad"] = severidad;
    p["fecha"] = fecha;
    String salida;
    serializeJson(doc, salida);
    ws.textAll(salida);

    emitirEstadoAlarma();
    (void)ok;
}

// =============================================================
//  PUERTA: SERVO + RFID
// =============================================================
void abrirPuerta() {
    cerradura.write(SERVO_ABIERTO);
    puertaAbiertaDesde = millis();
}

void cerrarPuertaSiToca() {
    if (puertaAbiertaDesde != 0 && millis() - puertaAbiertaDesde > TIEMPO_PUERTA_MS) {
        cerradura.write(SERVO_CERRADO);
        puertaAbiertaDesde = 0;
    }
}

String leerUID() {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

bool tarjetaAutorizada(const String &uid, String &nombreOut) {
    asegurarSql();
    if (!conn.connected()) return false;

    char q[160];
    snprintf(q, sizeof(q),
        "SELECT nombre FROM tarjetas_autorizadas WHERE uid='%s' AND habilitada=TRUE LIMIT 1",
        escapar(uid).c_str());

    MySQL_Query consulta = MySQL_Query(&conn);
    if (!consulta.execute(q)) return false;
    column_names *cols = consulta.get_columns();
    (void)cols;
    row_values *fila = consulta.get_next_row();
    if (fila != NULL && fila->values[0] != NULL) {
        nombreOut = String(fila->values[0]);
        return true;
    }
    return false;
}

void revisarRFID() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String uid = leerUID();
    String nombre = "";
    bool ok = tarjetaAutorizada(uid, nombre);

    if (ok) {
        Serial.printf("✅ Acceso autorizado: %s (%s)\n", nombre.c_str(), uid.c_str());
        abrirPuerta();
        disparar("RFID", "acceso_autorizado",
                 "Entrada de " + nombre, uid, "baja", 2, false);
    } else {
        Serial.printf("❌ Tarjeta no autorizada: %s\n", uid.c_str());
        disparar("RFID", "acceso_denegado",
                 "Intento de acceso con tarjeta desconocida", uid, "alta", 2);
    }

    rfid.PICC_HaltA();
}

// =============================================================
//  SIMULADOR EMBEBIDO (MODO_SIMULADOR == 1)
//  Genera alertas aleatorias para probar el sistema completo
//  sin tener sensores fisicos conectados.
// =============================================================
#if MODO_SIMULADOR
void simularLectura() {
    char valor[32];
    long n = random(11);
    switch (n) {
        case 0: {
            float t = 35.0f + random(0, 100) / 10.0f;
            snprintf(valor, sizeof(valor), "%.1f°C", t);
            disparar("DHT22", "temperatura_alta", "Temperatura elevada detectada", valor, "alta", 0);
            break;
        }
        case 1: {
            float h = 75.0f + random(0, 200) / 10.0f;
            snprintf(valor, sizeof(valor), "%.1f%%", h);
            disparar("DHT22", "humedad_alta", "Humedad muy elevada", valor, "media", 0);
            break;
        }
        case 2: {
            float t = 50.0f + random(0, 150) / 10.0f;
            snprintf(valor, sizeof(valor), "%.1f°C", t);
            disparar("DHT22", "temperatura_critica", "¡Temperatura critica! Posible incendio", valor, "critica", 0);
            break;
        }
        case 3: {
            int ppm = 400 + random(0, 400);
            snprintf(valor, sizeof(valor), "%d ppm", ppm);
            disparar("MQ2", "gas_detectado", "Concentracion de gas detectada", valor, "alta", 1);
            break;
        }
        case 4: {
            int ppm = 800 + random(0, 500);
            snprintf(valor, sizeof(valor), "%d ppm", ppm);
            disparar("MQ2", "gas_critico", "¡Fuga de gas peligrosa!", valor, "critica", 1);
            break;
        }
        case 5: {
            int ppm = 300 + random(0, 200);
            snprintf(valor, sizeof(valor), "%d ppm", ppm);
            disparar("MQ2", "humo_detectado", "Humo detectado en el ambiente", valor, "alta", 1);
            break;
        }
        case 6:
            abrirPuerta();
            disparar("RFID", "acceso_autorizado", "Entrada de Jose Eduardo", "A1B2C3D4", "baja", 2, false);
            break;
        case 7:
            disparar("RFID", "acceso_denegado", "Intento de acceso con tarjeta desconocida", "DEADBEEF", "alta", 2);
            break;
        case 8:
            disparar("RFID", "intentos_multiples", "Multiples intentos fallidos consecutivos", "00112233", "critica", 2);
            break;
        case 9:
            disparar("PIR", "movimiento_detectado", "Movimiento detectado en sala", "detectado", "media", 3);
            break;
        case 10:
            disparar("PIR", "movimiento_nocturno", "¡Movimiento detectado durante ausencia!", "detectado", "alta", 3);
            break;
    }
}
#endif

// =============================================================
//  LECTURA DE SENSORES
// =============================================================
void leerSensores() {
    // ----- DHT22 -----
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    bool dhtOk = !isnan(t) && !isnan(h);
    if (dhtOk) {
        // Temperatura y humedad son lecturas independientes: chequearlas por separado
        if (t >= TEMP_CRITICA) {
            disparar("DHT22", "temperatura_critica", "¡Temperatura critica! Posible incendio",
                     String(t, 1) + "°C", "critica", 0);
        } else if (t >= TEMP_ALTA) {
            disparar("DHT22", "temperatura_alta", "Temperatura elevada detectada",
                     String(t, 1) + "°C", "alta", 0);
        }
        if (h >= HUMEDAD_ALTA) {
            disparar("DHT22", "humedad_alta", "Humedad muy elevada",
                     String(h, 1) + "%", "media", 0);
        }
    }

    // ----- MQ-6 (salida digital activa en bajo) -----
    int mq2 = digitalRead(PIN_MQ2);
    if (mq2 == LOW) {
        disparar("MQ2", "gas_detectado", "Concentracion de gas detectada",
                 "umbral", "critica", 1);
    }

    // ----- PIR -----
    int pir = digitalRead(PIN_PIR);
    if (pir == HIGH && pirAnterior == LOW) {
        disparar("PIR", "movimiento_detectado", "Movimiento detectado",
                 "detectado", "media", 3);
    }
    pirAnterior = pir;

    // ----- DEBUG: imprimir lecturas crudas cada INTERVALO_DEBUG ms -----
    if (millis() - ultimoDebug >= INTERVALO_DEBUG) {
        ultimoDebug = millis();
        if (dhtOk) {
            Serial.printf("[lectura] DHT22 T=%.1f°C H=%.1f%% | MQ6 D0=%s | PIR=%s\n",
                          t, h,
                          mq2 == LOW ? "GAS(LOW)" : "limpio(HIGH)",
                          pir == HIGH ? "MOVIMIENTO" : "quieto");
        } else {
            Serial.printf("[lectura] DHT22 NaN (revisar cableado/pull-up) | MQ6 D0=%s | PIR=%s\n",
                          mq2 == LOW ? "GAS(LOW)" : "limpio(HIGH)",
                          pir == HIGH ? "MOVIMIENTO" : "quieto");
        }
    }
}

// =============================================================
//  WEBSOCKET HANDLER
// =============================================================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("🔌 Cliente WS conectado #%u\n", client->id());

        // Enviar estado actual
        StaticJsonDocument<128> est;
        est["tipo"] = "estado-alarma";
        JsonObject ep = est.createNestedObject("payload");
        ep["alarma_activa"] = alarmaActiva;
        String s;
        serializeJson(est, s);
        client->text(s);

        // Enviar historial
        DynamicJsonDocument hist(8192);
        hist["tipo"] = "historial";
        JsonArray arr = hist.createNestedArray("payload");
        cargarHistorial(arr);
        String sh;
        serializeJson(hist, sh);
        client->text(sh);

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("❌ Cliente WS desconectado #%u\n", client->id());

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char *)data);
            if (err) return;

            const char *tipo = doc["tipo"] | "";
            if (strcmp(tipo, "apagar-alarma") == 0) {
                apagarAlarmaPersistente();
            } else if (strcmp(tipo, "pedir-historial") == 0) {
                DynamicJsonDocument hist(8192);
                hist["tipo"] = "historial";
                JsonArray arr = hist.createNestedArray("payload");
                cargarHistorial(arr);
                String sh;
                serializeJson(hist, sh);
                client->text(sh);
            }
        }
    }
}

// =============================================================
//  BOTON FISICO
// =============================================================
void IRAM_ATTR isrBoton() {
    // Solo marcamos un timestamp; el procesamiento se hace en el loop
    ultimoBoton = millis();
}

void revisarBoton() {
    static unsigned long ultimoProcesado = 0;
    if (ultimoBoton != 0 && (millis() - ultimoBoton) > DEBOUNCE_MS) {
        if (ultimoBoton != ultimoProcesado) {
            ultimoProcesado = ultimoBoton;
            if (digitalRead(PIN_BOTON) == LOW && alarmaActiva) {
                Serial.println("Boton fisico: apagar alarma");
                apagarAlarmaPersistente();
            }
        }
    }
}

// =============================================================
//  WIFI Y FILESYSTEM
// =============================================================
void conectarWifi() {
    Serial.printf("Conectando a WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.printf("\n✅ WiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
}

void montarFs() {
    if (!LittleFS.begin(false)) {
        Serial.println("Formateando LittleFS...");
        LittleFS.begin(true);
    }
    Serial.println("✅ LittleFS montado");
}

// =============================================================
//  SETUP / LOOP
// =============================================================
void setup() {
    Serial.begin(115200);
    delay(200);

#if MODO_SIMULADOR
    Serial.printf("🤖 MODO SIMULADOR activo — alerta cada %d s\n", INTERVALO_SIMULADOR_MS / 1000);
#endif
    randomSeed(micros());

    pinMode(PIN_MQ2, INPUT);
    pinMode(PIN_PIR, INPUT);           // Igual que el sketch de prueba que ya valido: HC-SR501 maneja la linea activamente
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BOTON, INPUT_PULLUP);
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);

    attachInterrupt(digitalPinToInterrupt(PIN_BOTON), isrBoton, FALLING);

    dht.begin();

    SPI.begin();
    rfid.PCD_Init();
    cerradura.attach(PIN_SERVO);
    cerradura.write(SERVO_CERRADO);

    montarFs();
    conectarWifi();

    ipServidorSql.fromString(MYSQL_HOST);
    if (conectarSql()) {
        cargarEstadoAlarmaDesdeSql();
        controlActuadores();
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "No encontrado");
    });

    server.begin();
    Serial.println("🏠 Servidor HTTP/WS arriba en puerto 80");
    Serial.printf("Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
}

void loop() {
    ws.cleanupClients();
    revisarBoton();
    cerrarPuertaSiToca();

#if MODO_SIMULADOR
    if (millis() - ultimaLectura >= INTERVALO_SIMULADOR_MS) {
        ultimaLectura = millis();
        simularLectura();
    }
#else
    revisarRFID();
    if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
        ultimaLectura = millis();
        leerSensores();
    }
#endif
}
