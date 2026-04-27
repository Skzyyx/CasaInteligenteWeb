// app.js - Logica del dashboard (corre en el navegador, servido desde el ESP32)
// =============================================================================
// La comunicacion con el ESP32 se hace por un unico WebSocket en /ws con
// mensajes JSON tipados. No hay REST ni Socket.IO: el ESP32 sirve la UI
// estatica desde LittleFS y maneja la logica en C++.

// ===== CONEXION WEBSOCKET =====
let ws = null;
let reintentoMs = 1000;

function abrirWS() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    ws = new WebSocket(`${proto}://${location.host}/ws`);

    ws.addEventListener('open', () => {
        console.log('✅ Conectado al ESP32');
        el.indicadorConexion.classList.remove('desconectado');
        el.indicadorConexion.classList.add('conectado');
        el.textoConexion.textContent = 'En linea';
        reintentoMs = 1000;
    });

    ws.addEventListener('close', () => {
        console.log('❌ Desconectado, reintentando...');
        el.indicadorConexion.classList.remove('conectado');
        el.indicadorConexion.classList.add('desconectado');
        el.textoConexion.textContent = 'Desconectado';
        setTimeout(abrirWS, reintentoMs);
        reintentoMs = Math.min(reintentoMs * 2, 10000);
    });

    ws.addEventListener('message', (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        manejarMensaje(msg);
    });
}

function enviar(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(obj));
    }
}

// ===== REFERENCIAS AL DOM =====
const $ = (id) => document.getElementById(id);

const el = {
    indicadorConexion: $('indicador-conexion'),
    textoConexion: $('texto-conexion'),
    panelAlarma: $('panel-alarma'),
    alarmaEmoji: $('alarma-emoji'),
    alarmaEstado: $('alarma-estado'),
    alarmaSubtexto: $('alarma-subtexto'),
    btnApagar: $('btn-apagar'),
    listaAlertas: $('lista-alertas'),
    contadorAlertas: $('contador-alertas')
};

// ===== ESTADO =====
let alertas = [];
let alarmaActiva = false;

// ===== MAPEO VISUAL DE SENSORES =====
const ICONOS_SENSOR = {
    DHT22: '🌡️', DHT11: '🌡️', MQ2: '💨', REED: '🚪', PIR: '👁️', default: '📡'
};

const ESTADO_NORMAL_SENSOR = {
    DHT22: 'Normal', MQ2: 'Normal', REED: 'Cerrada', PIR: 'Sin actividad'
};

// ===== UTILIDADES =====
function formatearFecha(fechaStr) {
    if (!fechaStr) return '';
    const fecha = new Date(fechaStr.replace(' ', 'T'));
    const ahora = new Date();
    const diffSeg = Math.floor((ahora - fecha) / 1000);
    const diffMin = Math.floor(diffSeg / 60);
    const diffHoras = Math.floor(diffMin / 60);
    const diffDias = Math.floor(diffHoras / 24);
    if (diffSeg < 60) return 'hace unos segundos';
    if (diffMin < 60) return `hace ${diffMin} min`;
    if (diffHoras < 24) return `hace ${diffHoras}h`;
    if (diffDias < 7) return `hace ${diffDias}d`;
    return fecha.toLocaleDateString('es-MX');
}

// ===== RENDERIZADO =====
function crearItemAlerta(alerta) {
    const icono = ICONOS_SENSOR[alerta.sensor] || ICONOS_SENSOR.default;
    const severidad = alerta.severidad || 'media';
    const valorHtml = alerta.valor
        ? `<span class="alerta-sensor">📈 ${alerta.valor}</span>` : '';

    return `
    <div class="alerta-item sev-${severidad}" data-id="${alerta.id}">
      <div class="alerta-icono">${icono}</div>
      <div class="alerta-info">
        <div class="alerta-mensaje">${alerta.mensaje}</div>
        <div class="alerta-meta">
          <span class="alerta-sensor">${alerta.sensor}</span>
          <span>🕐 ${formatearFecha(alerta.fecha)}</span>
          ${valorHtml}
        </div>
      </div>
      <span class="alerta-severidad ${severidad}">${severidad}</span>
    </div>
  `;
}

function renderizarAlertas() {
    if (alertas.length === 0) {
        el.listaAlertas.innerHTML = `
      <div class="lista-vacia">
        <span>📭</span>
        <p>No hay alertas registradas</p>
      </div>`;
    } else {
        el.listaAlertas.innerHTML = alertas.map(crearItemAlerta).join('');
    }
    el.contadorAlertas.textContent = alertas.length;
}

function actualizarTarjetasSensores() {
    document.querySelectorAll('.sensor-card').forEach(card => {
        const sensor = card.dataset.sensor;
        card.classList.remove('alerta');
        const estadoEl = card.querySelector('.sensor-estado');
        if (estadoEl) estadoEl.textContent = ESTADO_NORMAL_SENSOR[sensor] || 'Normal';
    });

    if (alarmaActiva && alertas.length > 0) {
        const sensoresActivos = new Set();
        alertas.slice(0, 5).forEach(a => sensoresActivos.add(a.sensor));
        sensoresActivos.forEach(sensor => {
            const card = document.querySelector(`.sensor-card[data-sensor="${sensor}"]`);
            if (card) {
                card.classList.add('alerta');
                const estadoEl = card.querySelector('.sensor-estado');
                if (estadoEl) estadoEl.textContent = '¡ALERTA!';
            }
        });
    }
}

function actualizarEstadoAlarma(activa) {
    alarmaActiva = activa;
    if (activa) {
        el.panelAlarma.classList.remove('inactiva');
        el.panelAlarma.classList.add('activa');
        el.alarmaEmoji.textContent = '🚨';
        el.alarmaEstado.textContent = '¡ALARMA ACTIVA!';
        el.alarmaSubtexto.textContent = 'Se detecto una situacion que requiere tu atencion';
        el.btnApagar.classList.remove('oculto');
    } else {
        el.panelAlarma.classList.remove('activa');
        el.panelAlarma.classList.add('inactiva');
        el.alarmaEmoji.textContent = '✅';
        el.alarmaEstado.textContent = 'SISTEMA NORMAL';
        el.alarmaSubtexto.textContent = 'Todos los sensores operan correctamente';
        el.btnApagar.classList.add('oculto');
    }
    actualizarTarjetasSensores();
}

// ===== MANEJO DE MENSAJES DEL ESP32 =====
function manejarMensaje(msg) {
    if (!msg || !msg.tipo) return;

    if (msg.tipo === 'historial') {
        alertas = Array.isArray(msg.payload) ? msg.payload : [];
        renderizarAlertas();
        actualizarTarjetasSensores();

    } else if (msg.tipo === 'nueva-alerta') {
        alertas.unshift(msg.payload);
        if (alertas.length > 50) alertas.length = 50;
        renderizarAlertas();
        actualizarTarjetasSensores();

    } else if (msg.tipo === 'estado-alarma') {
        actualizarEstadoAlarma(Boolean(msg.payload && msg.payload.alarma_activa));
    }
}

// ===== ACCIONES DE UI =====
function apagarAlarma() {
    el.btnApagar.disabled = true;
    enviar({ tipo: 'apagar-alarma' });
    setTimeout(() => { el.btnApagar.disabled = false; }, 500);
}

el.btnApagar.addEventListener('click', apagarAlarma);

// ===== ARRANQUE =====
abrirWS();
