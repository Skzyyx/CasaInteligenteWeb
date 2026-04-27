# 🏠 Casa Inteligente — Sistema de Monitoreo IoT

Sistema de monitoreo y alertas en tiempo real para una casa inteligente, desarrollado como proyecto final de la materia **Sistemas Empotrados**. Todo el sistema corre dentro de un **ESP32 DOIT DEV KIT V1**: lectura de sensores, control de actuadores, servidor web con la UI servida desde **LittleFS** y comunicacion en tiempo real por WebSocket. La unica pieza externa es la base de datos **MySQL**.

---

## 📋 Tabla de contenidos

- [Descripción](#-descripción)
- [Arquitectura](#-arquitectura)
- [Características](#-características)
- [Stack tecnológico](#-stack-tecnológico)
- [Requisitos previos](#-requisitos-previos)
- [Instalación](#-instalación)
- [Probar sin hardware](#-probar-sin-hardware)
- [Protocolo WebSocket](#-protocolo-websocket)
- [Estructura del proyecto](#-estructura-del-proyecto)
- [Hardware](#-hardware)

---

## 📖 Descripción

El sistema monitorea cuatro tipos de sensores ambientales y de seguridad. Cuando alguno detecta una condicion anormal (temperatura excesiva, gas, apertura de puerta, movimiento), el ESP32:

1. Persiste la alerta en MySQL.
2. La transmite instantaneamente a todos los dashboards conectados via WebSocket.
3. Activa la alarma fisica (LED + buzzer).

Desde la pagina web —servida por el propio ESP32— se ve el historial, el estado en tiempo real y se puede apagar la alarma. Tambien existe un boton fisico en la protoboard para apagarla localmente.

---

## 🏗️ Arquitectura

```
┌──────────────────────────────────────────┐
│              ESP32 DOIT V1               │
│                                          │
│  ┌────────────┐    ┌──────────────────┐  │
│  │ 4 Sensores │    │ Servidor HTTP+WS │  │
│  │ DHT22, MQ2 │    │ (AsyncWebServer) │  │
│  │ REED, PIR  │    │                  │  │
│  └─────┬──────┘    │ UI desde         │  │
│        │           │ LittleFS         │  │
│  ┌─────▼──────┐    └─────────┬────────┘  │
│  │ Logica C++ │              │           │
│  │ (alertas)  │◄─────WS──────┤           │
│  └─────┬──────┘              │           │
│        │                     │           │
│  ┌─────▼──────┐              │           │
│  │ Actuadores │              │           │
│  │ LED+Buzzer │              │           │
│  └────────────┘              │           │
└────────┬─────────────────────┼───────────┘
         │ TCP 3306            │ HTTP/WS
         ▼                     ▼
   ┌──────────┐         ┌─────────────┐
   │  MySQL   │         │  Navegador  │
   │ (PC/LAN) │         │  Dashboard  │
   └──────────┘         └─────────────┘
```

### Flujo de datos

1. El ESP32 lee los sensores cada 2 segundos.
2. Al detectar una condicion de alerta, hace `INSERT` directo en MySQL via TCP/3306.
3. Inmediatamente despues, emite por WebSocket el evento `nueva-alerta` a todos los dashboards conectados.
4. El dashboard recibe el evento y actualiza la UI sin recargar.
5. El usuario puede apagar la alarma desde el dashboard (mensaje WS `apagar-alarma`) o desde el boton fisico.

---

## ✨ Características

- 🔌 **Todo corre en el ESP32** — UI, logica, comunicacion, persistencia (la BD es lo unico externo)
- 📡 **Comunicacion en tiempo real** mediante WebSocket nativo
- 💾 **Persistencia en MySQL** con historial completo de alertas
- 📁 **UI servida desde LittleFS** (HTML, CSS, JS embebidos en el ESP32)
- 🎨 **Dashboard moderno** con tema oscuro tipo panel de control
- 🚨 **Sistema de severidades** (baja, media, alta, critica)
- 🔕 **Doble control de alarma** (web o boton fisico)
- 📱 **Diseño responsive** para movil y desktop

---

## 🛠️ Stack tecnológico

### Firmware (ESP32, C++)
- **Arduino framework** (Arduino IDE o PlatformIO)
- **ESPAsyncWebServer** — servidor HTTP y WebSocket
- **AsyncTCP** — base de red asincrona
- **LittleFS** — sistema de archivos para servir la UI
- **MySQL_MariaDB_Generic** (khoih-prog) — cliente MySQL nativo
- **ArduinoJson** — serializacion de mensajes WS
- **DHT sensor library** (Adafruit) — DHT22

### Frontend (servido desde LittleFS)
- **HTML5 + CSS3** (vanilla)
- **JavaScript** (sin frameworks)
- **WebSocket API** del navegador

### Base de datos
- **MySQL 8+** (o MariaDB)

---

## 📦 Requisitos previos

- **ESP32 DOIT DEV KIT V1** (30 pines)
- **MySQL Server** o **MariaDB** corriendo en la LAN
- **PlatformIO** (recomendado) o **Arduino IDE** con soporte de ESP32
- Cliente MySQL para crear la BD (MySQL Workbench, DBeaver, phpMyAdmin, etc.)

---

## 🚀 Instalación

### 1. Configurar la base de datos

En tu servidor MySQL, ejecuta el siguiente script:

```sql
-- Crear la base de datos
CREATE DATABASE IF NOT EXISTS casa_inteligente
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE casa_inteligente;

-- Tabla de alertas (historial)
CREATE TABLE IF NOT EXISTS alertas (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sensor VARCHAR(50) NOT NULL,
  tipo VARCHAR(50) NOT NULL,
  mensaje VARCHAR(255) NOT NULL,
  valor VARCHAR(50),
  severidad ENUM('baja', 'media', 'alta', 'critica') NOT NULL DEFAULT 'media',
  fecha DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_fecha (fecha),
  INDEX idx_sensor (sensor)
);

-- Tabla de estado del sistema
CREATE TABLE IF NOT EXISTS estado_sistema (
  id INT PRIMARY KEY,
  alarma_activa BOOLEAN NOT NULL DEFAULT FALSE,
  actualizado DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- Fila inicial de estado
INSERT IGNORE INTO estado_sistema (id, alarma_activa) VALUES (1, FALSE);
```

### 2. Crear un usuario MySQL compatible con el ESP32

> ⚠️ **Importante:** la libreria `MySQL_MariaDB_Generic` **solo soporta `mysql_native_password`**. MySQL 8 usa por defecto `caching_sha2_password`, asi que hay que crear un usuario explicitamente con el plugin antiguo:

```sql
CREATE USER 'esp32'@'%' IDENTIFIED WITH mysql_native_password BY 'esp32pass';
GRANT ALL PRIVILEGES ON casa_inteligente.* TO 'esp32'@'%';
FLUSH PRIVILEGES;
```

Y asegura que MySQL acepta conexiones remotas. En `my.cnf` / `my.ini`:

```ini
bind-address = 0.0.0.0
```

Reinicia el servicio de MySQL y abre el puerto 3306 en el firewall.

### 3. Configurar credenciales del firmware

Copia la plantilla:

```bash
cp firmware/config.h.example firmware/config.h
```

Edita `firmware/config.h` con los datos de tu red y de MySQL:

```c
#define WIFI_SSID     "MiRedWiFi"
#define WIFI_PASS     "passwordwifi"

#define MYSQL_HOST    "192.168.1.100"   // IP de la PC con MySQL
#define MYSQL_PORT    3306
#define MYSQL_USER    "esp32"
#define MYSQL_PASS    "esp32pass"
#define MYSQL_DB      "casa_inteligente"
```

> El archivo `config.h` esta en `.gitignore` para que las credenciales no terminen en el repo.

### 4. Compilar y subir

#### Opcion A — PlatformIO (recomendado)

```bash
cd firmware

# Compilar y subir el sketch
pio run -t upload

# Subir la UI a LittleFS
pio run -t uploadfs

# Monitor serie
pio device monitor
```

#### Opcion B — Arduino IDE

1. Instala el soporte de ESP32 (Boards Manager).
2. Instala las librerias desde el Library Manager:
   - `ESPAsyncWebServer` (me-no-dev)
   - `AsyncTCP` (me-no-dev)
   - `ArduinoJson`
   - `DHT sensor library` (Adafruit)
   - `Adafruit Unified Sensor`
   - `MySQL_MariaDB_Generic`
3. Instala el plugin **ESP32 LittleFS Data Upload** para subir la carpeta `data/`.
4. Selecciona la board **ESP32 Dev Module**.
5. Sube el sketch (`Upload`) y luego sube los archivos de `data/` con **Tools → ESP32 LittleFS Data Upload**.

### 5. Abrir el dashboard

Una vez en marcha, el monitor serie mostrara la IP que el ESP32 tomo del DHCP:

```
✅ WiFi OK. IP: 192.168.1.55
✅ Conectado a MySQL
🏠 Servidor HTTP/WS arriba en puerto 80
Dashboard: http://192.168.1.55/
```

Abre esa URL en cualquier navegador conectado a la misma red.

---

## 🧪 Probar sin hardware

Mientras no tengas armado el circuito, el firmware incluye un **modo simulador embebido** que genera alertas aleatorias dentro del propio ESP32. Pasas exactamente por el mismo flujo (alerta → INSERT en MySQL → broadcast por WebSocket → render en el dashboard) pero sin sensores conectados.

### Activarlo

En `firmware/config.h` (que copiaste de la plantilla):

```c
#define MODO_SIMULADOR          1     // 1 = simulador, 0 = sensores reales
#define INTERVALO_SIMULADOR_MS  8000  // milisegundos entre alertas
```

### Lo que necesitas

- ESP32 conectado por USB (es lo unico fisico).
- WiFi configurado en `config.h`.
- MySQL accesible desde la red del ESP32 (ya configurado en los pasos anteriores).

### Que hace

- Cada `INTERVALO_SIMULADOR_MS` el ESP32 escoge aleatoriamente uno de los 11 escenarios de alerta (DHT22, MQ2, REED, PIR con sus variantes y severidades) y dispara la alerta como si viniera de un sensor real.
- Salta el cooldown por sensor para que las demos fluyan sin huecos.
- El boton fisico sigue funcionando (apagar alarma) pero ya no es necesario; tambien lo apagas desde la UI.

### Cuando llegue el hardware

Cambia `MODO_SIMULADOR` a `0`, recompila y vuelve a subir el sketch. Sin tocar nada mas.

---

## 🔌 Protocolo WebSocket

Toda la comunicacion entre el navegador y el ESP32 va por un solo WebSocket en `ws://<ip-esp32>/ws`. Los mensajes son JSON con el campo `tipo`.

### Del ESP32 al navegador

| Tipo | Payload | Cuando |
|---|---|---|
| `historial` | `[ {alerta}, ... ]` | Al conectar (ultimas 50 alertas) o al pedirlo |
| `nueva-alerta` | `{ id, sensor, tipo, mensaje, valor, severidad, fecha }` | Sensor dispara una alerta |
| `estado-alarma` | `{ alarma_activa: bool }` | Cambia el estado de la alarma |

### Del navegador al ESP32

| Tipo | Cuando |
|---|---|
| `apagar-alarma` | Usuario presiona el boton de apagar |
| `pedir-historial` | Refrescar la lista manualmente |

---

## 📁 Estructura del proyecto

```
CasaInteligenteWeb/
├── firmware/
│   ├── casa_inteligente.ino    # Sketch principal del ESP32
│   ├── platformio.ini          # Configuracion de PlatformIO
│   ├── config.h.example        # Plantilla de credenciales
│   ├── config.h                # Credenciales reales (NO subir a Git)
│   └── data/                   # Se sube a LittleFS
│       ├── index.html          # Dashboard
│       ├── css/
│       │   └── style.css       # Estilos
│       └── js/
│           └── app.js          # Logica del dashboard (WebSocket)
├── .gitignore
├── LICENSE
└── README.md
```

---

## 🔧 Hardware

### Componentes utilizados

| Componente | Modelo | Funcion |
|---|---|---|
| Microcontrolador | ESP32 DOIT DEV KIT V1 (30 pines) | Cerebro del sistema |
| Sensor de temperatura/humedad | DHT22 | Deteccion de temperatura alta |
| Sensor de gas | MQ-2 | Deteccion de gas / humo |
| Sensor magnetico | Reed switch + iman | Deteccion de apertura de puerta |
| Sensor de movimiento | PIR HC-SR501 | Deteccion de presencia |
| Actuador visual | LED rojo 5mm | Alarma visual |
| Actuador sonoro | Buzzer activo 5V | Alarma sonora |
| Control local | Push button | Apagar alarma localmente |

### Asignación de pines (ESP32)

| Componente | GPIO | Tipo |
|---|---|---|
| DHT22 | GPIO 4 | Digital |
| MQ-2 (DO) | GPIO 34 | Digital in |
| Reed switch | GPIO 27 | Digital in (pull-up) |
| PIR HC-SR501 | GPIO 26 | Digital in |
| LED rojo | GPIO 2 | Digital out |
| Buzzer | GPIO 15 | Digital out |
| Boton reset | GPIO 14 | Digital in (pull-up, interrupt) |

---

## 📝 Licencia

Proyecto academico desarrollado para la materia de **Sistemas Empotrados**.
