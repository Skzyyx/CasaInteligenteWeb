// db.js - Configuración de conexión a MySQL
require('dotenv').config();
const mysql = require('mysql2/promise');

// Creamos un "pool" de conexiones en lugar de una sola conexión.
// Un pool reutiliza conexiones y maneja múltiples queries en paralelo
// sin abrir/cerrar conexiones constantemente. Es la forma correcta
// de trabajar con BD en un servidor web.
const pool = mysql.createPool({
    host: process.env.DB_HOST,
    port: process.env.DB_PORT,
    user: process.env.DB_USER,
    password: process.env.DB_PASSWORD,
    database: process.env.DB_NAME,
    waitForConnections: true,
    connectionLimit: 10,      // máx 10 conexiones simultáneas
    queueLimit: 0,
    dateStrings: true         // devuelve fechas como string en vez de objeto Date
    // (más fácil de enviar al frontend como JSON)
});

// Función para probar la conexión al iniciar el servidor
async function probarConexion() {
    try {
        const conexion = await pool.getConnection();
        console.log('✅ Conexión a MySQL exitosa');
        conexion.release(); // devolvemos la conexión al pool
        return true;
    } catch (error) {
        console.error('❌ Error al conectar a MySQL:', error.message);
        return false;
    }
}

module.exports = { pool, probarConexion };