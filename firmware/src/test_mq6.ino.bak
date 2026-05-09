// =============================================================
//  TEST MQ-6 (sensor de gas LPG / butano / propano)
// =============================================================
//  Sketch aislado para probar SOLO el sensor MQ-6.
//  Lee el pin digital D0 y avisa cuando cambia su estado.
//
//  CONEXION:
//    MQ6 VCC -> 5V (VIN del ESP32)  ¡IMPORTANTE: 5V, no 3.3!
//    MQ6 GND -> GND
//    MQ6 D0  -> GPIO 34  (D34 en la placa)
//    MQ6 A0  -> sin conectar (no lo usamos)
//
//  CALIBRACION DEL TRIMPOT (cubito azul del modulo):
//    1) Espera 3 minutos a que se caliente.
//    2) En aire limpio, gira el trimpot HORARIO hasta que el
//       LED 'D-OUT' del modulo se APAGUE.
//    3) Acerca un encendedor (sin prender) a 5cm:
//         - Si el LED D-OUT enciende -> sensor calibrado OK
//         - Si NO enciende -> gira ANTIHORARIO un poco mas
//
//  LECTURA:
//    D0 = HIGH -> aire limpio (sin gas)
//    D0 = LOW  -> gas detectado
// =============================================================

#include <Arduino.h> 
#include <WiFi.h>  // necesario porque PIO arrastra WebServer.h transitivamente

#define PIN_MQ6 34

int d0Ant = HIGH;
unsigned long ultimoEstado = 0;
unsigned long inicio = 0;
unsigned long contadorGas = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(PIN_MQ6, INPUT);

    Serial.println();
    Serial.println("================================");
    Serial.println("   TEST MQ-6 (sensor de gas)");
    Serial.println("================================");
    Serial.println("Pin D0 del MQ-6  -> GPIO 34");
    Serial.println("Alimentacion     -> 5V (no 3.3V)");
    Serial.println();
    Serial.println("Calentando sensor (180 segundos)...");
    Serial.println("El MQ-6 necesita unos 3 minutos para estabilizarse.");
    Serial.println("Manten el sensor en aire limpio durante este tiempo.");
    Serial.println();

    // Lee periodicamente durante el warm-up para ver evolucion
    for (int i = 180; i > 0; i--) {
        if (i % 10 == 0) {
            int d0 = digitalRead(PIN_MQ6);
            Serial.printf("[warm-up] %ds restantes  D0=%s\n",
                          i, d0 == LOW ? "LOW (detecta gas)" : "HIGH (limpio)");
        }
        delay(1000);
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("Calentado. Ya puedes probar.");
    Serial.println("Acerca un encendedor (SIN prender) a 5cm del sensor.");
    Serial.println("Despues alejate y ventila para ver que vuelve a HIGH.");
    Serial.println("================================");
    inicio = millis();
}

void loop() {
    int d0 = digitalRead(PIN_MQ6);
    unsigned long t = (millis() - inicio) / 1000;

    // Detectar flanco HIGH -> LOW = gas detectado
    if (d0 == LOW && d0Ant == HIGH) {
        contadorGas++;
        Serial.printf("[%lus] >>> GAS DETECTADO #%lu (D0=LOW)\n", t, contadorGas);
    }
    // Detectar flanco LOW -> HIGH = aire limpio de nuevo
    else if (d0 == HIGH && d0Ant == LOW) {
        Serial.printf("[%lus] <<< aire limpio de nuevo (D0=HIGH)\n", t);
    }
    d0Ant = d0;

    // Imprimir estado actual cada 3 segundos
    if (millis() - ultimoEstado >= 3000) {
        ultimoEstado = millis();
        Serial.printf("[%lus] estado del pin: %s\n",
                      t,
                      d0 == LOW ? "LOW (gas)" : "HIGH (limpio)");
    }

    delay(20);
}
