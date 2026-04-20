// simulador.js - Simula un ESP32 enviando alertas al backend
// ============================================================
// Modos:
//   node simulador.js              → modo automatico (por defecto)
//   node simulador.js manual       → modo manual (comandos por teclado)
//   node simulador.js auto 5       → automatico, alertas cada 5 segundos

const fetch = require('node-fetch');
const readline = require('readline');

// ===== CONFIGURACIÓN =====
const API_URL = 'http://localhost:3000';
const modo = process.argv[2] || 'auto';
const intervaloAuto = parseInt(process.argv[3]) || 15; // segundos entre alertas en modo auto

// ===== CATÁLOGO DE ALERTAS POSIBLES =====
// Cada sensor tiene varios escenarios que puede reportar
const ESCENARIOS = {
    DHT22: [
        { tipo: 'temperatura_alta', mensaje: 'Temperatura elevada detectada', valor: () => `${(35 + Math.random() * 10).toFixed(1)}°C`, severidad: 'alta' },
        { tipo: 'humedad_alta', mensaje: 'Humedad muy elevada', valor: () => `${(75 + Math.random() * 20).toFixed(1)}%`, severidad: 'media' },
        { tipo: 'temperatura_critica', mensaje: '¡Temperatura crítica! Posible incendio', valor: () => `${(50 + Math.random() * 15).toFixed(1)}°C`, severidad: 'critica' }
    ],
    MQ2: [
        { tipo: 'gas_detectado', mensaje: 'Concentración de gas detectada', valor: () => `${Math.floor(400 + Math.random() * 400)} ppm`, severidad: 'alta' },
        { tipo: 'gas_critico', mensaje: '¡Fuga de gas peligrosa!', valor: () => `${Math.floor(800 + Math.random() * 500)} ppm`, severidad: 'critica' },
        { tipo: 'humo_detectado', mensaje: 'Humo detectado en el ambiente', valor: () => `${Math.floor(300 + Math.random() * 200)} ppm`, severidad: 'alta' }
    ],
    REED: [
        { tipo: 'puerta_abierta', mensaje: 'Puerta principal abierta', valor: () => 'abierta', severidad: 'media' },
        { tipo: 'puerta_forzada', mensaje: '¡Posible intrusión detectada!', valor: () => 'forzada', severidad: 'critica' },
        { tipo: 'ventana_abierta', mensaje: 'Ventana de cocina abierta', valor: () => 'abierta', severidad: 'baja' }
    ],
    PIR: [
        { tipo: 'movimiento_detectado', mensaje: 'Movimiento detectado en sala', valor: () => 'detectado', severidad: 'media' },
        { tipo: 'movimiento_nocturno', mensaje: '¡Movimiento detectado durante ausencia!', valor: () => 'detectado', severidad: 'alta' }
    ]
};

// ===== UTILIDADES =====
function log(emoji, mensaje, color = '') {
    const colores = {
        verde: '\x1b[32m',
        rojo: '\x1b[31m',
        amarillo: '\x1b[33m',
        cyan: '\x1b[36m',
        gris: '\x1b[90m',
        reset: '\x1b[0m'
    };
    const c = colores[color] || '';
    const timestamp = new Date().toLocaleTimeString('es-MX');
    console.log(`${colores.gris}[${timestamp}]${colores.reset} ${c}${emoji} ${mensaje}${colores.reset}`);
}

function aleatorio(array) {
    return array[Math.floor(Math.random() * array.length)];
}

// ===== ENVIAR ALERTA AL BACKEND =====
async function enviarAlerta(sensor, escenario) {
    const payload = {
        sensor,
        tipo: escenario.tipo,
        mensaje: escenario.mensaje,
        valor: typeof escenario.valor === 'function' ? escenario.valor() : escenario.valor,
        severidad: escenario.severidad
    };

    try {
        const res = await fetch(`${API_URL}/api/alert`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (data.ok) {
            log('📤', `Alerta enviada: [${sensor}] ${payload.mensaje} (${payload.valor})`, 'verde');
        } else {
            log('❌', `Error del servidor: ${JSON.stringify(data)}`, 'rojo');
        }
    } catch (error) {
        log('❌', `No se pudo conectar al backend: ${error.message}`, 'rojo');
        log('💡', 'Asegúrate de que el servidor esté corriendo en ' + API_URL, 'amarillo');
    }
}

// ===== CONSULTAR ESTADO DE ALARMA (simula polling del ESP32) =====
let estadoAlarmaAnterior = null;
async function consultarEstadoAlarma() {
    try {
        const res = await fetch(`${API_URL}/api/estado-alarma`);
        const data = await res.json();

        // Solo logueamos cuando cambia el estado, para no llenar la consola
        if (data.alarma_activa !== estadoAlarmaAnterior) {
            if (data.alarma_activa) {
                log('🚨', 'ESP32: alarma ACTIVA → LED + buzzer ENCENDIDOS', 'rojo');
            } else if (estadoAlarmaAnterior !== null) {
                log('🔕', 'ESP32: alarma APAGADA → LED + buzzer APAGADOS', 'cyan');
            }
            estadoAlarmaAnterior = data.alarma_activa;
        }
    } catch (error) {
        // silenciamos errores de polling para no llenar consola
    }
}

// ===== MODO AUTOMÁTICO =====
function modoAutomatico() {
    log('🤖', `Modo AUTOMÁTICO activado — alerta cada ${intervaloAuto}s`, 'cyan');
    log('💡', 'Presiona Ctrl+C para detener\n', 'amarillo');

    const sensores = Object.keys(ESCENARIOS);

    setInterval(() => {
        const sensor = aleatorio(sensores);
        const escenario = aleatorio(ESCENARIOS[sensor]);
        enviarAlerta(sensor, escenario);
    }, intervaloAuto * 1000);
}

// ===== MODO MANUAL =====
function modoManual() {
    const rl = readline.createInterface({ input: process.stdin, output: process.stdout });

    function mostrarMenu() {
        console.log('\n┌─────────────────────────────────────────┐');
        console.log('│        SIMULADOR ESP32 - MANUAL         │');
        console.log('├─────────────────────────────────────────┤');
        console.log('│  1 → DHT22 (temperatura/humedad)        │');
        console.log('│  2 → MQ2 (gas/humo)                     │');
        console.log('│  3 → REED (puerta)                      │');
        console.log('│  4 → PIR (movimiento)                   │');
        console.log('│  5 → Alerta aleatoria                   │');
        console.log('│  q → Salir                              │');
        console.log('└─────────────────────────────────────────┘');
    }

    function preguntar() {
        rl.question('Comando: ', async (cmd) => {
            cmd = cmd.trim().toLowerCase();

            const mapa = { '1': 'DHT22', '2': 'MQ2', '3': 'REED', '4': 'PIR' };

            if (mapa[cmd]) {
                const sensor = mapa[cmd];
                const escenario = aleatorio(ESCENARIOS[sensor]);
                await enviarAlerta(sensor, escenario);
            } else if (cmd === '5') {
                const sensor = aleatorio(Object.keys(ESCENARIOS));
                const escenario = aleatorio(ESCENARIOS[sensor]);
                await enviarAlerta(sensor, escenario);
            } else if (cmd === 'q' || cmd === 'quit' || cmd === 'exit') {
                log('👋', 'Hasta luego', 'cyan');
                rl.close();
                process.exit(0);
            } else if (cmd === '') {
                // ignorar
            } else {
                log('❓', 'Comando no reconocido', 'amarillo');
                mostrarMenu();
            }

            preguntar();
        });
    }

    log('🎮', 'Modo MANUAL activado', 'cyan');
    mostrarMenu();
    preguntar();
}

// ===== INICIAR =====
async function iniciar() {
    console.log('\n╔═══════════════════════════════════════╗');
    console.log('║    🤖 SIMULADOR ESP32 - Casa Smart    ║');
    console.log('╚═══════════════════════════════════════╝\n');

    // Verificar conexión con el backend
    log('🔍', `Verificando conexión con ${API_URL}...`, 'gris');
    try {
        const res = await fetch(`${API_URL}/api/health`);
        const data = await res.json();
        if (data.status === 'ok') {
            log('✅', 'Backend funcionando correctamente\n', 'verde');
        }
    } catch (error) {
        log('❌', `No se pudo conectar al backend en ${API_URL}`, 'rojo');
        log('💡', 'Asegúrate de haber ejecutado "npm run dev" en otra terminal', 'amarillo');
        process.exit(1);
    }

    // Polling del estado de alarma (como haría el ESP32 real)
    setInterval(consultarEstadoAlarma, 2000);

    // Arrancar según el modo elegido
    if (modo === 'manual' || modo === 'm') {
        modoManual();
    } else {
        modoAutomatico();
    }
}

iniciar();