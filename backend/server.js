// server.js - Servidor principal de la Casa Inteligente (con Socket.IO)
require('dotenv').config();
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
const path = require('path');
const { pool, probarConexion } = require('./db');

const app = express();
const PORT = process.env.PORT || 3000;

// ===== CONFIGURACIÓN DE SOCKET.IO =====
// Express por sí solo no soporta WebSockets, por eso creamos un
// servidor HTTP nativo y le "montamos" Express encima. Luego
// conectamos Socket.IO a ese mismo servidor HTTP.
const server = http.createServer(app);
const io = new Server(server, {
    cors: { origin: '*' } // permitimos cualquier origen para desarrollo
});

// ===== MIDDLEWARES =====
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// ===== ENDPOINTS DE LA API =====

// --- Healthcheck ---
app.get('/api/health', async (req, res) => {
    try {
        await pool.query('SELECT 1');
        res.json({ status: 'ok', mensaje: 'Servidor y BD funcionando' });
    } catch (error) {
        res.status(500).json({ status: 'error', mensaje: error.message });
    }
});

// --- POST /api/alert: el ESP32 envía una alerta ---
app.post('/api/alert', async (req, res) => {
    try {
        const { sensor, tipo, mensaje, valor, severidad } = req.body;

        if (!sensor || !tipo || !mensaje) {
            return res.status(400).json({
                error: 'Faltan campos requeridos: sensor, tipo, mensaje'
            });
        }

        const [resultado] = await pool.query(
            `INSERT INTO alertas (sensor, tipo, mensaje, valor, severidad)
       VALUES (?, ?, ?, ?, ?)`,
            [sensor, tipo, mensaje, valor || null, severidad || 'media']
        );

        await pool.query(
            `UPDATE estado_sistema SET alarma_activa = TRUE WHERE id = 1`
        );

        const [filas] = await pool.query(
            `SELECT * FROM alertas WHERE id = ?`,
            [resultado.insertId]
        );
        const alertaCreada = filas[0];

        console.log(`🚨 Nueva alerta [${sensor}]: ${mensaje}`);

        // 🔥 NUEVO: emitir evento a TODOS los dashboards conectados
        io.emit('nueva-alerta', alertaCreada);
        io.emit('estado-alarma', { alarma_activa: true });

        res.status(201).json({ ok: true, alerta: alertaCreada });
    } catch (error) {
        console.error('Error al guardar alerta:', error);
        res.status(500).json({ error: 'Error interno del servidor' });
    }
});

// --- GET /api/alerts: historial ---
app.get('/api/alerts', async (req, res) => {
    try {
        const limite = parseInt(req.query.limite) || 50;
        const [alertas] = await pool.query(
            `SELECT * FROM alertas ORDER BY fecha DESC LIMIT ?`,
            [limite]
        );
        res.json({ ok: true, alertas });
    } catch (error) {
        console.error('Error al obtener alertas:', error);
        res.status(500).json({ error: 'Error interno del servidor' });
    }
});

// --- GET /api/estado-alarma ---
app.get('/api/estado-alarma', async (req, res) => {
    try {
        const [filas] = await pool.query(
            `SELECT alarma_activa FROM estado_sistema WHERE id = 1`
        );
        res.json({ ok: true, alarma_activa: Boolean(filas[0].alarma_activa) });
    } catch (error) {
        console.error('Error al consultar estado:', error);
        res.status(500).json({ error: 'Error interno del servidor' });
    }
});

// --- POST /api/alarma/apagar ---
app.post('/api/alarma/apagar', async (req, res) => {
    try {
        await pool.query(
            `UPDATE estado_sistema SET alarma_activa = FALSE WHERE id = 1`
        );

        console.log('🔕 Alarma apagada');

        // 🔥 NUEVO: avisar a todos los dashboards
        io.emit('estado-alarma', { alarma_activa: false });

        res.json({ ok: true, mensaje: 'Alarma apagada' });
    } catch (error) {
        console.error('Error al apagar alarma:', error);
        res.status(500).json({ error: 'Error interno del servidor' });
    }
});

// ===== EVENTOS DE SOCKET.IO =====
// Esto se ejecuta cada vez que un dashboard se conecta
io.on('connection', (socket) => {
    console.log(`🔌 Cliente conectado: ${socket.id}`);

    socket.on('disconnect', () => {
        console.log(`❌ Cliente desconectado: ${socket.id}`);
    });
});

// ===== INICIAR SERVIDOR =====
async function iniciar() {
    const conectado = await probarConexion();
    if (!conectado) {
        console.error('No se pudo conectar a la BD. Abortando...');
        process.exit(1);
    }

    // ⚠️ Usamos server.listen (no app.listen) porque Socket.IO
    // necesita engancharse al servidor HTTP nativo
    server.listen(PORT, () => {
        console.log(`🏠 Servidor Casa Inteligente corriendo en http://localhost:${PORT}`);
        console.log(`📊 Dashboard: http://localhost:${PORT}`);
        console.log(`🔌 API: http://localhost:${PORT}/api`);
        console.log(`📡 WebSockets habilitados`);
    });
}

iniciar();