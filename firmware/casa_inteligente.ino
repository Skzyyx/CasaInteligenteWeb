// =============================================================
//  Casa Inteligente - Firmware ESP32
// =============================================================
//  Funciones:
//   - Lectura de sensores (DHT22, MQ-2, REED, PIR)
//   - Activacion de actuadores (LED, buzzer)
//   - Servidor web sirviendo la UI desde LittleFS
//   - WebSocket para comunicacion en tiempo real con el dashboard
//   - Persistencia de alertas en MySQL externo
// =============================================================

#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <MySQL_Generic.h>
#include "config.h"

// ===== PINES =====
#define PIN_DHT       4
#define PIN_MQ2      34
#define PIN_REED     27
#define PIN_PIR      26
#define PIN_LED       2
#define PIN_BUZZER   15
#define PIN_BOTON    14

#define DHT_TIPO   DHT22

// ===== UMBRALES =====
#define TEMP_ALTA          35.0    // grados Celsius
#define TEMP_CRITICA       50.0
#define HUMEDAD_ALTA       80.0    // porcentaje
#define DEBOUNCE_MS        300

// Intervalo entre lecturas de sensores
#define INTERVALO_LECTURA  2000    // ms
// Cooldown entre alertas del mismo sensor para no saturar la BD
#define COOLDOWN_ALERTA    10000   // ms

// ===== OBJETOS GLOBALES =====
DHT dht(PIN_DHT, DHT_TIPO);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiClient clienteSql;
MySQL_Connection conn((Client *)&clienteSql);
IPAddress ipServidorSql;

// ===== ESTADO =====
volatile bool alarmaActiva = false;
unsigned long ultimaLectura = 0;
unsigned long ultimaAlerta[4] = { 0, 0, 0, 0 }; // DHT22, MQ2, REED, PIR
unsigned long ultimoBoton = 0;

// Estado anterior de los sensores digitales para detectar flancos
int reedAnterior = HIGH;
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

    MYSQL_LOGLN("Conectando a MySQL...");
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
                   const String &valor, const String &severidad, long &idGenerado, String &fechaOut) {
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

    // Marcar la alarma como activa
    MySQL_Query updEstado = MySQL_Query(&conn);
    updEstado.execute("UPDATE estado_sistema SET alarma_activa = TRUE WHERE id = 1");

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
              const String &valor, const String &severidad, int idxCooldown) {
    unsigned long ahora = millis();
    if (ahora - ultimaAlerta[idxCooldown] < COOLDOWN_ALERTA) return;
    ultimaAlerta[idxCooldown] = ahora;

    long id = 0;
    String fecha = "";
    bool ok = guardarAlerta(sensor, tipo, mensaje, valor, severidad, id, fecha);

    Serial.printf("🚨 [%s] %s (%s)\n", sensor.c_str(), mensaje.c_str(), valor.c_str());

    alarmaActiva = true;
    controlActuadores();

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
//  LECTURA DE SENSORES
// =============================================================
void leerSensores() {
    // ----- DHT22 -----
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        if (t >= TEMP_CRITICA) {
            disparar("DHT22", "temperatura_critica", "¡Temperatura critica! Posible incendio",
                     String(t, 1) + "°C", "critica", 0);
        } else if (t >= TEMP_ALTA) {
            disparar("DHT22", "temperatura_alta", "Temperatura elevada detectada",
                     String(t, 1) + "°C", "alta", 0);
        } else if (h >= HUMEDAD_ALTA) {
            disparar("DHT22", "humedad_alta", "Humedad muy elevada",
                     String(h, 1) + "%", "media", 0);
        }
    }

    // ----- MQ-2 (salida digital activa en bajo) -----
    int mq2 = digitalRead(PIN_MQ2);
    if (mq2 == LOW) {
        disparar("MQ2", "gas_detectado", "Concentracion de gas detectada",
                 "umbral", "critica", 1);
    }

    // ----- REED (cerrada = LOW con pull-up + iman) -----
    int reed = digitalRead(PIN_REED);
    if (reed == HIGH && reedAnterior == LOW) {
        disparar("REED", "puerta_abierta", "Puerta principal abierta",
                 "abierta", "media", 2);
    }
    reedAnterior = reed;

    // ----- PIR -----
    int pir = digitalRead(PIN_PIR);
    if (pir == HIGH && pirAnterior == LOW) {
        disparar("PIR", "movimiento_detectado", "Movimiento detectado",
                 "detectado", "media", 3);
    }
    pirAnterior = pir;
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

    pinMode(PIN_MQ2, INPUT);
    pinMode(PIN_REED, INPUT_PULLUP);
    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BOTON, INPUT_PULLUP);
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);

    attachInterrupt(digitalPinToInterrupt(PIN_BOTON), isrBoton, FALLING);

    dht.begin();
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

    if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
        ultimaLectura = millis();
        leerSensores();
    }
}
