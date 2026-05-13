// =============================================================
//  config.h - Credenciales del firmware
// =============================================================
//  Copiar este archivo a "config.h" y rellenar con los valores
//  reales. config.h NO debe subirse al repositorio.
// =============================================================

#ifndef CONFIG_H
#define CONFIG_H
//jose
// ----- WiFi -----
#define WIFI_SSID     "INFINITUMEEA1_2.4"
#define WIFI_PASS     "Ea8Hf5Vr8x"

// ----- MySQL -----
// IP de la maquina donde corre MySQL en la LAN.
// El usuario debe estar creado con mysql_native_password.
#define MYSQL_HOST    "192.168.1.67"
#define MYSQL_PORT    3306
#define MYSQL_USER    "esp32"
#define MYSQL_PASS    "esp32pass"
#define MYSQL_DB      "casa_inteligente"

// ----- MODO SIMULADOR -----
// Permite probar el sistema completo SIN tener los sensores conectados.
// Cuando esta en 1, el ESP32 ignora los pines de sensores y genera
// alertas aleatorias por si mismo (igual que hacia el simulador en Node).
// Cambia a 0 cuando ya tengas el circuito armado.
#define MODO_SIMULADOR          0
#define INTERVALO_SIMULADOR_MS  8000   // milisegundos entre alertas simuladas

// ----- CALIBRACION SERVO -----
// Cuando esta en 1, el firmware se SALTA toda la logica normal y solo barre
// pulsos del servo (1300-1700 us) para encontrar el "STOP" real de servos
// de rotacion continua. Observa el monitor serie y anota en que valor se
// queda quieto. Despues poner en 0.
#define CALIBRACION_SERVO       0

#endif
