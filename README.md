# 🏠 Casa Inteligente — Sistema de Monitoreo IoT

Sistema de monitoreo y alertas en tiempo real para una casa inteligente, desarrollado como proyecto final de la materia **Sistemas Empotrados**. Utiliza un microcontrolador **ESP32 DOIT DEV KIT V1** para recolectar datos de múltiples sensores y reportar eventos críticos a un panel de control web.

---

## 📋 Tabla de contenidos

- [Descripción](#-descripción)
- [Arquitectura](#-arquitectura)
- [Características](#-características)
- [Stack tecnológico](#-stack-tecnológico)
- [Requisitos previos](#-requisitos-previos)
- [Instalación](#-instalación)
- [Uso](#-uso)
- [Endpoints de la API](#-endpoints-de-la-api)
- [Estructura del proyecto](#-estructura-del-proyecto)
- [Hardware](#-hardware)
- [Simulador del ESP32](#-simulador-del-esp32)
- [Capturas](#-capturas)
- [Roadmap](#-roadmap)

---

## 📖 Descripción

El sistema monitorea cuatro tipos de sensores ambientales y de seguridad en un entorno doméstico. Cuando alguno de los sensores detecta una condición anormal (temperatura excesiva, gas, apertura de puerta, movimiento), el ESP32 envía una alerta al backend, que la persiste en una base de datos y la transmite instantáneamente al dashboard web mediante WebSockets. Desde la página web es posible visualizar el historial, consultar el estado en tiempo real y apagar la alarma física de forma remota.

---

## 🏗️ Arquitectura

```
┌─────────────────────┐         WiFi          ┌──────────────────────┐
│   ESP32 DOIT V1     │◄──────HTTP POST──────►│   Backend Node.js    │
│                     │                        │   (PC local)         │
│  ┌──────────────┐   │                        │                      │
│  │ 4 Sensores   │   │                        │  ┌────────────────┐  │
│  │ - DHT22      │   │                        │  │ Express + REST │  │
│  │ - MQ-2       │   │                        │  │ Socket.IO      │  │
│  │ - Reed       │   │                        │  │ MySQL          │  │
│  │ - PIR        │   │                        │  └───────┬────────┘  │
│  └──────────────┘   │                        │          │           │
│                     │                        └──────────┼───────────┘
│  ┌──────────────┐   │                                   │ WebSocket
│  │ Actuadores   │   │◄──────HTTP GET────────────────────┤
│  │ - LED rojo   │   │    (polling estado)               │
│  │ - Buzzer     │   │                                   ▼
│  │ - Botón reset│   │                         ┌─────────────────┐
│  └──────────────┘   │                         │  Dashboard web  │
└─────────────────────┘                         │   (navegador)   │
                                                └─────────────────┘
```

### Flujo de datos

1. El ESP32 lee los sensores en tiempo real.
2. Al detectar una condición de alerta, envía un `POST /api/alert` al backend.
3. El backend persiste la alerta en MySQL y emite un evento por Socket.IO.
4. Todos los dashboards conectados reciben la alerta instantáneamente.
5. El ESP32 consulta periódicamente el estado de la alarma (`GET /api/estado-alarma`) para saber si debe seguir activando el LED y el buzzer.
6. El usuario puede apagar la alarma desde el dashboard o desde un botón físico en la protoboard.

---

## ✨ Características

- 📡 **Comunicación en tiempo real** mediante WebSockets (Socket.IO)
- 💾 **Persistencia en MySQL** con historial completo de alertas
- 🎨 **Dashboard moderno** con tema oscuro tipo panel de control
- 🔄 **Actualización automática** del estado de los sensores sin recargar
- 🚨 **Sistema de severidades** (baja, media, alta, crítica)
- 🔕 **Control remoto** de la alarma física desde la web
- 🤖 **Simulador integrado** que emula al ESP32 para desarrollo y demos
- 📱 **Diseño responsive** para móvil y desktop

---

## 🛠️ Stack tecnológico

### Backend
- **Node.js** v24+
- **Express** — framework web
- **Socket.IO** — comunicación en tiempo real
- **mysql2** — driver de MySQL con soporte de Promises
- **dotenv** — manejo de variables de entorno
- **cors** — habilita peticiones del ESP32

### Frontend
- **HTML5 + CSS3** (vanilla)
- **JavaScript** (sin frameworks)
- **Socket.IO Client**

### Base de datos
- **MySQL 8+**

### Hardware (por implementar)
- **ESP32 DOIT DEV KIT V1** (30 pines)
- **Arduino IDE** / **PlatformIO**

---

## 📦 Requisitos previos

Antes de instalar, asegúrate de tener:

- [Node.js](https://nodejs.org/) v18 o superior
- [MySQL Server](https://dev.mysql.com/downloads/) o XAMPP
- [Git](https://git-scm.com/)
- Cliente MySQL (MySQL Workbench, phpMyAdmin, DBeaver, etc.)

Verifica las versiones:

```bash
node --version    # v18+
npm --version     # v9+
mysql --version   # 8+
```

---

## 🚀 Instalación

### 1. Clonar el repositorio

```bash
git clone https://github.com/TU_USUARIO/CasaInteligenteWeb.git
cd CasaInteligenteWeb/backend
```

### 2. Instalar dependencias

```bash
npm install
```

### 3. Configurar la base de datos

Abre MySQL Workbench (o tu cliente preferido) y ejecuta el siguiente script:

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

### 4. Configurar variables de entorno

Crea un archivo `.env` en la carpeta `backend/` con el siguiente contenido:

```env
# Servidor
PORT=3000

# MySQL
DB_HOST=localhost
DB_PORT=3306
DB_USER=root
DB_PASSWORD=tu_password_de_mysql
DB_NAME=casa_inteligente
```

> ⚠️ **Importante:** el archivo `.env` contiene tu contraseña de MySQL. **Nunca lo subas a Git**. El `.gitignore` ya está configurado para ignorarlo.

### 5. Iniciar el servidor

**Modo desarrollo** (con auto-reload al modificar archivos):

```bash
npm run dev
```

**Modo producción:**

```bash
npm start
```

Si todo está bien configurado, deberías ver:

```
✅ Conexión a MySQL exitosa
🏠 Servidor Casa Inteligente corriendo en http://localhost:3000
📊 Dashboard: http://localhost:3000
🔌 API: http://localhost:3000/api
📡 WebSockets habilitados
```

### 6. Abrir el dashboard

Abre en tu navegador:

```
http://localhost:3000
```

---

## 🎮 Uso

### Dashboard web

El dashboard muestra:

- **Estado general del sistema** (normal / alarma activa)
- **Tarjetas de sensores** que cambian de color cuando detectan una alerta
- **Historial de alertas** ordenado cronológicamente con severidades
- **Botón de apagado** de la alarma que se habilita cuando hay una alerta activa

### Simulador del ESP32

Mientras no tengas el hardware físico, puedes usar el simulador para probar el sistema completo.

**Modo automático** (envía alertas aleatorias cada 15 segundos):

```bash
node simulador.js
```

**Modo automático con intervalo personalizado** (ej: cada 3 segundos):

```bash
node simulador.js auto 3
```

**Modo manual** (disparas alertas a voluntad desde el teclado):

```bash
node simulador.js manual
```

En modo manual verás un menú interactivo:

```
┌─────────────────────────────────────────┐
│        SIMULADOR ESP32 - MANUAL         │
├─────────────────────────────────────────┤
│  1 → DHT22 (temperatura/humedad)        │
│  2 → MQ2 (gas/humo)                     │
│  3 → REED (puerta)                      │
│  4 → PIR (movimiento)                   │
│  5 → Alerta aleatoria                   │
│  q → Salir                              │
└─────────────────────────────────────────┘
```

---

## 🔌 Endpoints de la API

### `GET /api/health`
Verifica que el servidor y la base de datos estén funcionando.

**Respuesta:**
```json
{ "status": "ok", "mensaje": "Servidor y BD funcionando" }
```

### `POST /api/alert`
Recibe una alerta desde el ESP32 (o el simulador).

**Body:**
```json
{
  "sensor": "MQ2",
  "tipo": "gas_detectado",
  "mensaje": "Nivel de gas peligroso",
  "valor": "850 ppm",
  "severidad": "critica"
}
```

**Respuesta:** `201 Created` con la alerta registrada.

### `GET /api/alerts`
Devuelve el historial de alertas.

**Query params opcionales:**
- `limite` — número máximo de alertas (default: 50)

**Respuesta:**
```json
{
  "ok": true,
  "alertas": [ { "id": 1, "sensor": "MQ2", ... } ]
}
```

### `GET /api/estado-alarma`
Consulta si la alarma física debe estar activa. Lo usa el ESP32 mediante polling.

**Respuesta:**
```json
{ "ok": true, "alarma_activa": true }
```

### `POST /api/alarma/apagar`
Apaga la alarma desde el dashboard o desde el botón físico del ESP32.

**Respuesta:**
```json
{ "ok": true, "mensaje": "Alarma apagada" }
```

### Eventos de Socket.IO

**Del servidor al cliente:**
- `nueva-alerta` — se emite cuando entra una alerta nueva
- `estado-alarma` — se emite cuando cambia el estado de la alarma

---

## 📁 Estructura del proyecto

```
CasaInteligenteWeb/
├── backend/
│   ├── public/                    # Frontend (servido estáticamente)
│   │   ├── css/
│   │   │   └── style.css          # Estilos del dashboard
│   │   ├── js/
│   │   │   └── app.js             # Lógica del cliente
│   │   └── index.html             # Dashboard
│   ├── .env                       # Variables de entorno (NO subir a Git)
│   ├── .gitignore
│   ├── db.js                      # Configuración de conexión a MySQL
│   ├── server.js                  # Servidor principal (Express + Socket.IO)
│   ├── simulador.js               # Simulador del ESP32
│   ├── package.json
│   └── package-lock.json
├── firmware/                      # Código del ESP32 (próximamente)
│   └── casa_inteligente.ino
└── README.md
```

---

## 🔧 Hardware

### Componentes utilizados

| Componente | Modelo | Función |
|---|---|---|
| Microcontrolador | ESP32 DOIT DEV KIT V1 (30 pines) | Cerebro del sistema |
| Sensor de temperatura/humedad | DHT22 | Detección de temperatura alta |
| Sensor de gas | MQ-2 | Detección de gas / humo |
| Sensor magnético | Reed switch + imán | Detección de apertura de puerta |
| Sensor de movimiento | PIR HC-SR501 | Detección de presencia |
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
| Botón reset | GPIO 14 | Digital in (pull-up) |

---

## 🤖 Simulador del ESP32

El simulador emula fielmente el comportamiento del ESP32 real:

- Envía alertas a `POST /api/alert` como lo haría el microcontrolador
- Hace polling cada 2 segundos a `GET /api/estado-alarma` para conocer el estado de la alarma
- Genera valores realistas para cada sensor (ppm de gas, °C, %, etc.)
- Soporta los 4 sensores con múltiples escenarios de alerta por cada uno
- Muestra logs en consola con timestamps y colores

Esto permite desarrollar y presentar el sistema completo **sin necesidad del hardware**.

---

## 🗺️ Roadmap

- [x] Backend con API REST
- [x] Base de datos MySQL con persistencia
- [x] WebSockets para tiempo real
- [x] Dashboard web con tema oscuro
- [x] Simulador del ESP32
- [ ] Firmware del ESP32
- [ ] Armado físico del circuito
- [ ] Pruebas con sensores reales
- [ ] Documentación de instalación del hardware

---

## 📝 Licencia

Proyecto académico desarrollado para la materia de **Sistemas Empotrados**.

---

## 👤 Autor

Desarrollado como proyecto final universitario.