// app.js - Lógica del dashboard
// ================================================

// Conexión con Socket.IO (se conecta automáticamente al mismo host)
const socket = io();

// ===== REFERENCIAS AL DOM (IDs reales del HTML) =====
const $ = (id) => document.getElementById(id);

const el = {
    // Conexión
    indicadorConexion: $('indicador-conexion'),
    textoConexion: $('texto-conexion'),

    // Panel de alarma
    panelAlarma: $('panel-alarma'),
    alarmaEmoji: $('alarma-emoji'),
    alarmaEstado: $('alarma-estado'),
    alarmaSubtexto: $('alarma-subtexto'),
    btnApagar: $('btn-apagar'),

    // Historial
    listaAlertas: $('lista-alertas'),
    contadorAlertas: $('contador-alertas')
};

// ===== ESTADO =====
let alertas = [];
let alarmaActiva = false;

// ===== MAPEO VISUAL DE SENSORES =====
const ICONOS_SENSOR = {
    DHT22: '🌡️',
    DHT11: '🌡️',
    MQ2: '💨',
    REED: '🚪',
    PIR: '👁️',
    default: '📡'
};

const NOMBRE_SENSOR = {
    DHT22: 'Temperatura/Humedad',
    DHT11: 'Temperatura/Humedad',
    MQ2: 'Gas/Humo',
    REED: 'Puerta/Ventana',
    PIR: 'Movimiento',
    default: 'Sensor'
};

// Estados normales de cada sensor (para cuando la alerta termina)
const ESTADO_NORMAL_SENSOR = {
    DHT22: 'Normal',
    MQ2: 'Normal',
    REED: 'Cerrada',
    PIR: 'Sin actividad'
};

// ===== FUNCIONES AUXILIARES =====

function formatearFecha(fechaStr) {
    const fecha = new Date(fechaStr.replace(' ', 'T'));
    const ahora = new Date();
    const diffMs = ahora - fecha;
    const diffSeg = Math.floor(diffMs / 1000);
    const diffMin = Math.floor(diffSeg / 60);
    const diffHoras = Math.floor(diffMin / 60);
    const diffDias = Math.floor(diffHoras / 24);

    if (diffSeg < 60) return 'hace unos segundos';
    if (diffMin < 60) return `hace ${diffMin} min`;
    if (diffHoras < 24) return `hace ${diffHoras}h`;
    if (diffDias < 7) return `hace ${diffDias}d`;
    return fecha.toLocaleDateString('es-MX');
}

// ===== RENDERIZAR UNA ALERTA =====
function crearItemAlerta(alerta) {
    const icono = ICONOS_SENSOR[alerta.sensor] || ICONOS_SENSOR.default;
    const severidad = alerta.severidad || 'media';
    const valorHtml = alerta.valor
        ? `<span class="alerta-sensor">📈 ${alerta.valor}</span>`
        : '';

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

// ===== RENDERIZAR LISTA =====
function renderizarAlertas() {
    if (alertas.length === 0) {
        el.listaAlertas.innerHTML = `
      <div class="lista-vacia">
        <span>📭</span>
        <p>No hay alertas registradas</p>
      </div>
    `;
    } else {
        el.listaAlertas.innerHTML = alertas.map(crearItemAlerta).join('');
    }

    el.contadorAlertas.textContent = alertas.length;
}

// ===== MARCAR SENSORES CON ALERTA ACTIVA =====
// Hace que la tarjeta del sensor correspondiente se ponga en rojo
function actualizarTarjetasSensores() {
    // Resetear todas las tarjetas
    document.querySelectorAll('.sensor-card').forEach(card => {
        const sensor = card.dataset.sensor;
        card.classList.remove('alerta');
        const estadoEl = card.querySelector('.sensor-estado');
        if (estadoEl) estadoEl.textContent = ESTADO_NORMAL_SENSOR[sensor] || 'Normal';
    });

    // Si la alarma está activa, marcamos los sensores que dispararon recientemente
    if (alarmaActiva && alertas.length > 0) {
        // Tomamos los sensores únicos de las últimas alertas (mientras la alarma esté activa)
        const sensoresActivos = new Set();
        // Solo consideramos las alertas "recientes" (las que dispararon la alarma actual)
        // En este caso tomamos las últimas 5 como referencia
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

// ===== ESTADO DE LA ALARMA =====
function actualizarEstadoAlarma(activa) {
    alarmaActiva = activa;

    if (activa) {
        el.panelAlarma.classList.remove('inactiva');
        el.panelAlarma.classList.add('activa');
        el.alarmaEmoji.textContent = '🚨';
        el.alarmaEstado.textContent = '¡ALARMA ACTIVA!';
        el.alarmaSubtexto.textContent = 'Se detectó una situación que requiere tu atención';
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

// ===== CARGAR DATOS INICIALES =====
async function cargarDatosIniciales() {
    try {
        const resAlertas = await fetch('/api/alerts');
        const dataAlertas = await resAlertas.json();
        alertas = dataAlertas.alertas || [];
        renderizarAlertas();

        const resEstado = await fetch('/api/estado-alarma');
        const dataEstado = await resEstado.json();
        actualizarEstadoAlarma(dataEstado.alarma_activa);
    } catch (error) {
        console.error('Error al cargar datos iniciales:', error);
        el.listaAlertas.innerHTML = `
      <div class="lista-vacia">
        <span>⚠️</span>
        <p>Error al cargar alertas</p>
      </div>
    `;
    }
}

// ===== APAGAR ALARMA =====
async function apagarAlarma() {
    el.btnApagar.disabled = true;
    try {
        const res = await fetch('/api/alarma/apagar', { method: 'POST' });
        const data = await res.json();
        if (!data.ok) throw new Error('No se pudo apagar');
        // El estado se actualizará por Socket.IO automáticamente
    } catch (error) {
        console.error('Error al apagar alarma:', error);
        alert('Error al apagar la alarma. Intenta de nuevo.');
    } finally {
        el.btnApagar.disabled = false;
    }
}

// ===== EVENTOS DE SOCKET.IO =====
socket.on('connect', () => {
    console.log('✅ Conectado al servidor');
    el.indicadorConexion.classList.remove('desconectado');
    el.indicadorConexion.classList.add('conectado');
    el.textoConexion.textContent = 'En línea';
});

socket.on('disconnect', () => {
    console.log('❌ Desconectado del servidor');
    el.indicadorConexion.classList.remove('conectado');
    el.indicadorConexion.classList.add('desconectado');
    el.textoConexion.textContent = 'Desconectado';
});

socket.on('nueva-alerta', (alerta) => {
    console.log('🚨 Nueva alerta recibida:', alerta);
    alertas.unshift(alerta);
    renderizarAlertas();
    actualizarTarjetasSensores();
});

socket.on('estado-alarma', (data) => {
    console.log('🔔 Estado de alarma:', data);
    actualizarEstadoAlarma(data.alarma_activa);
});

// ===== EVENTOS DE UI =====
el.btnApagar.addEventListener('click', apagarAlarma);

// ===== ARRANQUE =====
cargarDatosIniciales();