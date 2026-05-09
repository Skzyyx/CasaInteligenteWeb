"""
Genera el diagrama de conexiones de la Casa Inteligente
con los pines actualizados: PIR=27, Servo=13, DHT11, MQ-6.
"""
import os
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle, FancyArrowPatch
from matplotlib.lines import Line2D
import numpy as np

# ===== Colores de cable =====
C_VCC = "#d62728"      # rojo - 3.3V/5V
C_GND = "#000000"      # negro - GND
C_LED = "#2ca02c"      # verde - GPIO 2
C_BUZZ = "#9467bd"     # morado - GPIO 15
C_BOTON = "#17becf"    # cian - GPIO 14
C_DHT = "#ff7f0e"      # naranja - GPIO 4
C_MQ = "#8c564b"       # marron - GPIO 34
C_PIR = "#e377c2"      # rosa - GPIO 27 (PIR)
C_SERVO = "#ff7f0e"    # naranja oscuro - GPIO 13 (servo)

# Colores SPI (RC522)
C_SDA = "#7f7f7f"
C_SCK = "#1f77b4"
C_MOSI = "#bcbd22"
C_MISO = "#17a2b8"
C_RST = "#e76f51"

fig, ax = plt.subplots(figsize=(16, 10))
ax.set_xlim(0, 16)
ax.set_ylim(0, 10)
ax.set_aspect('equal')
ax.axis('off')

# ===== Titulo =====
ax.text(8, 9.6, "Casa Inteligente — Diagrama de conexiones (ESP32 DOIT V1)",
        ha='center', fontsize=14, fontweight='bold')
ax.text(8, 9.25, "Pines actualizados: PIR=GPIO27, Servo=GPIO13, DHT11 (3.3V), MQ-6 (5V)",
        ha='center', fontsize=9.5, style='italic', color='#555')

# ===== ESP32 DOIT V1 =====
esp_x, esp_y = 0.4, 1.5
esp_w, esp_h = 2.6, 7.0
ax.add_patch(FancyBboxPatch((esp_x, esp_y), esp_w, esp_h,
                            boxstyle="round,pad=0.05",
                            facecolor='#1a1f2e', edgecolor='#444', linewidth=1.5))
ax.text(esp_x + esp_w/2, esp_y + esp_h + 0.15, "ESP32 DOIT V1",
        ha='center', fontsize=10, fontweight='bold')

# ESP32 WROOM module (centro de la placa)
ax.add_patch(Rectangle((esp_x + 0.55, esp_y + 4.2), 1.5, 1.6,
                       facecolor='#2a3142', edgecolor='#666'))
ax.text(esp_x + esp_w/2, esp_y + 5.0, "ESP32\nWROOM",
        ha='center', va='center', fontsize=7, color='#bbb')

# Pin labels (lado izquierdo y derecho)
# Lista de pines: (label, lado, y_offset_relativo)
# Lado: 'L' = izquierda (los GPIO ALTOS), 'R' = derecha (los BAJOS)
pines_izq = [
    ("EN",  6.6),
    ("VP",  6.2),
    ("VN",  5.8),
    ("34",  5.4),  # MQ-6
    ("35",  5.0),
    ("32",  4.6),
    ("33",  4.2),
    ("25",  3.8),
    ("26",  3.4),
    ("27",  3.0),  # PIR
    ("14",  2.6),  # Boton
    ("12",  2.2),
    ("13",  1.8),  # Servo
    ("GND", 1.4),
]
pines_der = [
    ("3V3", 6.6),
    ("GND", 6.2),
    ("15",  5.8),  # Buzzer
    ("2",   5.4),  # LED
    ("4",   5.0),  # DHT11
    ("16",  4.6),
    ("17",  4.2),
    ("5",   3.8),  # RC522 SDA
    ("18",  3.4),  # RC522 SCK
    ("19",  3.0),  # RC522 MISO
    ("21",  2.6),
    ("RX",  2.2),
    ("TX",  1.8),
    ("22",  1.4),  # RC522 RST
    ("23",  1.0),  # RC522 MOSI
]

# Pines usados (resaltados)
pines_usados = {"34", "27", "14", "13", "GND_L", "3V3", "GND_R", "15", "2", "4",
                "5", "18", "19", "22", "23"}

def dibujar_pin(label, x, y, lado):
    # Cuadradito amarillo si es pin usado
    key = label
    if label == "GND" and lado == 'L':
        key = "GND_L"
    elif label == "GND" and lado == 'R':
        key = "GND_R"
    usado = key in pines_usados
    color_pin = "#f4c430" if usado else "#444"
    if lado == 'L':
        ax.add_patch(Rectangle((x - 0.13, y - 0.07), 0.13, 0.14,
                               facecolor=color_pin, edgecolor='#222'))
        ax.text(x + 0.05, y, label, ha='left', va='center',
                fontsize=7, color='white' if not usado else '#fff',
                fontweight='bold' if usado else 'normal')
    else:
        ax.add_patch(Rectangle((x, y - 0.07), 0.13, 0.14,
                               facecolor=color_pin, edgecolor='#222'))
        ax.text(x - 0.05, y, label, ha='right', va='center',
                fontsize=7, color='white' if not usado else '#fff',
                fontweight='bold' if usado else 'normal')

# Posiciones x para los pines (laterales del rectangulo del ESP32)
x_pin_izq = esp_x + 0.05
x_pin_der = esp_x + esp_w - 0.18

for label, ry in pines_izq:
    dibujar_pin(label, x_pin_izq, esp_y + ry, 'L')
for label, ry in pines_der:
    dibujar_pin(label, x_pin_der, esp_y + ry, 'R')

# USB conector a la izquierda
ax.add_patch(Rectangle((esp_x - 0.35, esp_y + 4.5), 0.4, 0.6,
                       facecolor='#888', edgecolor='#555'))
ax.text(esp_x - 0.15, esp_y + 4.8, "USB", ha='center', va='center',
        fontsize=6, color='white')

# ===== Protoboard =====
pb_x, pb_y = 4.0, 1.8
pb_w, pb_h = 11.0, 5.5
ax.add_patch(FancyBboxPatch((pb_x, pb_y), pb_w, pb_h,
                            boxstyle="round,pad=0.08",
                            facecolor='#f5f0e1', edgecolor='#c9b892', linewidth=1.2))

# Carriles de alimentacion (lineas roja y negra arriba y abajo)
# Arriba
ax.plot([pb_x + 0.2, pb_x + pb_w - 0.2], [pb_y + pb_h - 0.45, pb_y + pb_h - 0.45],
        color='red', linewidth=1)
ax.plot([pb_x + 0.2, pb_x + pb_w - 0.2], [pb_y + pb_h - 0.7, pb_y + pb_h - 0.7],
        color='blue', linewidth=1)
ax.text(pb_x + 0.1, pb_y + pb_h - 0.45, "+", color='red', fontsize=10, fontweight='bold', va='center')
ax.text(pb_x + 0.1, pb_y + pb_h - 0.7, "−", color='blue', fontsize=10, fontweight='bold', va='center')
# Abajo
ax.plot([pb_x + 0.2, pb_x + pb_w - 0.2], [pb_y + 0.7, pb_y + 0.7],
        color='red', linewidth=1)
ax.plot([pb_x + 0.2, pb_x + pb_w - 0.2], [pb_y + 0.45, pb_y + 0.45],
        color='blue', linewidth=1)
ax.text(pb_x + 0.1, pb_y + 0.7, "+", color='red', fontsize=10, fontweight='bold', va='center')
ax.text(pb_x + 0.1, pb_y + 0.45, "−", color='blue', fontsize=10, fontweight='bold', va='center')

# Linea central del gap
ax.plot([pb_x + 0.5, pb_x + pb_w - 0.5], [pb_y + pb_h/2, pb_y + pb_h/2],
        color='#999', linewidth=0.5, linestyle=':')
ax.text(pb_x + pb_w/2, pb_y + pb_h/2 + 0.05, "(gap central)",
        ha='center', fontsize=6, color='#888', style='italic')

# Patron de hoyitos (decoración leve)
for col in np.arange(pb_x + 0.45, pb_x + pb_w - 0.4, 0.18):
    for row_y in np.arange(pb_y + 0.95, pb_y + pb_h - 0.85, 0.18):
        # Saltarse zona del gap central
        if abs(row_y - (pb_y + pb_h/2)) < 0.15:
            continue
        ax.plot(col, row_y, 'o', color='#d4c8a8', markersize=1.2)

# ===== COMPONENTES en el protoboard =====

# --- LED rojo + resistencia ---
led_x, led_y = pb_x + 1.0, pb_y + pb_h - 1.7
ax.add_patch(Circle((led_x, led_y), 0.18, facecolor='#e74c3c', edgecolor='#900', linewidth=1))
ax.text(led_x, led_y + 0.4, "LED", ha='center', fontsize=8, fontweight='bold')
# Resistencia
ax.add_patch(Rectangle((led_x + 0.3, led_y - 0.07), 0.5, 0.14,
                       facecolor='#d4a373', edgecolor='#8b5a2b'))
ax.text(led_x + 0.55, led_y - 0.3, "220Ω", ha='center', fontsize=6)
# Patas
ax.plot([led_x - 0.05, led_x - 0.05], [led_y - 0.18, led_y - 0.7],
        color='black', linewidth=1.2)  # GND
ax.plot([led_x + 0.05, led_x + 0.05], [led_y - 0.18, led_y - 0.45],
        color='black', linewidth=1.2)
ax.plot([led_x + 0.05, led_x + 0.3], [led_y - 0.45, led_y - 0.45],
        color='black', linewidth=1.2)

# --- BOTON (a horcajadas sobre el gap central, como en una protoboard real) ---
bot_x, bot_y = pb_x + 2.4, pb_y + pb_h/2
# Cuerpo del boton centrado sobre el gap
ax.add_patch(Rectangle((bot_x - 0.22, bot_y - 0.22), 0.44, 0.44,
                       facecolor='#222', edgecolor='#000', linewidth=1.2, zorder=3))
ax.add_patch(Circle((bot_x, bot_y), 0.09, facecolor='#666', zorder=4))
# 4 patas: 2 arriba (lado superior del gap) y 2 abajo (lado inferior)
for dx in (-0.16, 0.16):
    # Patas superiores (van al lado superior del gap)
    ax.plot([bot_x + dx, bot_x + dx], [bot_y + 0.22, bot_y + 0.42],
            color='#aaa', linewidth=1.5, zorder=2)
    ax.plot(bot_x + dx, bot_y + 0.42, 'o', color='silver', markersize=2.5, zorder=2)
    # Patas inferiores (van al lado inferior del gap)
    ax.plot([bot_x + dx, bot_x + dx], [bot_y - 0.22, bot_y - 0.42],
            color='#aaa', linewidth=1.5, zorder=2)
    ax.plot(bot_x + dx, bot_y - 0.42, 'o', color='silver', markersize=2.5, zorder=2)
ax.text(bot_x, bot_y + 0.6, "BOTÓN", ha='center', fontsize=8, fontweight='bold')

# --- DHT11 (modulo azul) ---
dht_x, dht_y = pb_x + 4.0, pb_y + pb_h - 1.7
ax.add_patch(Rectangle((dht_x - 0.4, dht_y - 0.3), 0.8, 0.6,
                       facecolor='#3a7cb8', edgecolor='#1f4e79', linewidth=1))
ax.add_patch(Circle((dht_x, dht_y + 0.05), 0.18, facecolor='#5a98c8', edgecolor='#1f4e79'))
ax.text(dht_x, dht_y + 0.5, "DHT11", ha='center', fontsize=8, fontweight='bold', color='#1f4e79')
# 3 patas (en modulo: VCC, DATA, GND)
for i, (lbl, off) in enumerate([("VCC", -0.2), ("DATA", 0), ("GND", 0.2)]):
    ax.plot(dht_x + off, dht_y - 0.4, 'o', color='silver', markersize=3)
    ax.text(dht_x + off, dht_y - 0.55, lbl, ha='center', fontsize=5)

# --- MQ-6 ---
mq_x, mq_y = pb_x + 5.8, pb_y + pb_h - 1.7
ax.add_patch(Rectangle((mq_x - 0.4, mq_y - 0.3), 0.8, 0.6,
                       facecolor='#f4a261', edgecolor='#a05a2c', linewidth=1))
ax.add_patch(Circle((mq_x, mq_y + 0.05), 0.16, facecolor='#d4843c', edgecolor='#7c4019'))
ax.text(mq_x, mq_y + 0.5, "MQ-6", ha='center', fontsize=8, fontweight='bold', color='#7c4019')
for i, (lbl, off) in enumerate([("VCC", -0.25), ("GND", -0.08), ("DO", 0.08), ("AO", 0.25)]):
    ax.plot(mq_x + off, mq_y - 0.4, 'o', color='silver', markersize=3)
    ax.text(mq_x + off, mq_y - 0.55, lbl, ha='center', fontsize=5)

# --- PIR HC-SR501 ---
pir_x, pir_y = pb_x + 7.6, pb_y + pb_h - 1.7
ax.add_patch(Rectangle((pir_x - 0.35, pir_y - 0.3), 0.7, 0.6,
                       facecolor='#90ee90', edgecolor='#3a7d3a', linewidth=1))
ax.add_patch(Circle((pir_x, pir_y + 0.05), 0.2, facecolor='#fffacd', edgecolor='#666'))
ax.text(pir_x, pir_y + 0.5, "PIR", ha='center', fontsize=8, fontweight='bold', color='#3a7d3a')
for i, (lbl, off) in enumerate([("VCC", -0.18), ("OUT", 0), ("GND", 0.18)]):
    ax.plot(pir_x + off, pir_y - 0.4, 'o', color='silver', markersize=3)
    ax.text(pir_x + off, pir_y - 0.55, lbl, ha='center', fontsize=5)

# --- BUZZER ---
buz_x, buz_y = pb_x + 3.0, pb_y + 1.2
ax.add_patch(Circle((buz_x, buz_y), 0.22, facecolor='#222', edgecolor='#000', linewidth=1.2))
ax.add_patch(Circle((buz_x, buz_y), 0.08, facecolor='#555'))
ax.text(buz_x, buz_y - 0.45, "BUZZER", ha='center', fontsize=8, fontweight='bold')

# --- RC522 (RFID) ---
rc_x, rc_y = pb_x + 9.0, pb_y + 1.4
ax.add_patch(Rectangle((rc_x - 0.5, rc_y - 0.4), 1.0, 0.8,
                       facecolor='#e2849c', edgecolor='#a64d6d', linewidth=1))
# Antena del RFID
ax.add_patch(Rectangle((rc_x - 0.4, rc_y - 0.1), 0.8, 0.4,
                       facecolor='none', edgecolor='#a64d6d', linewidth=0.6))
ax.text(rc_x, rc_y + 0.55, "RC522 (RFID 3.3V)", ha='center', fontsize=8, fontweight='bold', color='#7c2d4a')
ax.text(rc_x, rc_y + 0.1, "Antena", ha='center', fontsize=6, color='#7c2d4a', style='italic')
for i, (lbl, off) in enumerate([("SDA", -0.42), ("SCK", -0.28), ("MOSI", -0.14),
                                 ("MISO", 0), ("IRQ", 0.14), ("GND", 0.28),
                                 ("RST", 0.42)]):
    ax.plot(rc_x + off, rc_y - 0.5, 'o', color='silver', markersize=2.5)
    ax.text(rc_x + off, rc_y - 0.65, lbl, ha='center', fontsize=4.5)
# Pin 3.3V extra a la derecha
ax.plot(rc_x + 0.55, rc_y - 0.5, 'o', color='silver', markersize=2.5)
ax.text(rc_x + 0.55, rc_y - 0.65, "3.3V", ha='center', fontsize=4.5)

# --- SERVO SG90 (a la derecha, fuera del protoboard) ---
srv_x, srv_y = pb_x + pb_w + 0.4, pb_y + pb_h/2 - 0.3
ax.add_patch(FancyBboxPatch((srv_x - 0.05, srv_y - 0.5), 0.55, 1.0,
                            boxstyle="round,pad=0.02",
                            facecolor='#3a7d3a', edgecolor='#1f4f1f', linewidth=1))
ax.text(srv_x + 0.22, srv_y + 0.7, "SERVO SG90", ha='center', fontsize=8, fontweight='bold')
ax.text(srv_x + 0.22, srv_y + 0.55, "(fuente 5V externa)", ha='center', fontsize=6, style='italic', color='#555')
# 3 cables del servo
ax.text(srv_x + 0.55, srv_y + 0.2, "Naranja(señal)", fontsize=5.5, color='#ff7f0e')
ax.text(srv_x + 0.55, srv_y, "Rojo(+5V)", fontsize=5.5, color='red')
ax.text(srv_x + 0.55, srv_y - 0.2, "Marrón(GND)", fontsize=5.5, color='#5a3a1f')

# ===== CABLES (lineas de conexion) =====
# Helper: dibujar cable de pin del ESP32 a un punto en el protoboard
def cable(x1, y1, x2, y2, color, lw=1.4, alpha=0.85, zorder=2):
    # Trazado en L (con un waypoint)
    midx = (x1 + x2) / 2
    ax.plot([x1, midx, midx, x2], [y1, y1, y2, y2],
            color=color, linewidth=lw, alpha=alpha, zorder=zorder,
            solid_capstyle='round')

# 3V3 (esp pin x_pin_der + 0.13, esp_y + 6.6) → carril rojo arriba
cable(x_pin_der + 0.13, esp_y + 6.6, pb_x + 0.5, pb_y + pb_h - 0.45, C_VCC)
# GND (R) → carril azul arriba
cable(x_pin_der + 0.13, esp_y + 6.2, pb_x + 0.7, pb_y + pb_h - 0.7, C_GND)

# GPIO 2 (LED) — pin der "2" en y=esp_y+5.4
cable(x_pin_der + 0.13, esp_y + 5.4, led_x + 0.05, led_y - 0.18, C_LED)
# GND del LED al carril GND (negro abajo)
ax.plot([led_x - 0.05, led_x - 0.05], [led_y - 0.18, pb_y + 0.45],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 15 (Buzzer)
cable(x_pin_der + 0.13, esp_y + 5.8, buz_x + 0.1, buz_y, C_BUZZ)
ax.plot([buz_x - 0.1, buz_x - 0.1], [buz_y - 0.2, pb_y + 0.45],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 14 (Boton) — pin izq
# Entra por la pata superior izquierda; el GND sale por la pata inferior derecha
cable(x_pin_izq, esp_y + 2.6, bot_x - 0.16, bot_y + 0.42, C_BOTON)
ax.plot([bot_x + 0.16, bot_x + 0.16], [bot_y - 0.42, pb_y + 0.45],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 4 (DHT11 DATA)
cable(x_pin_der + 0.13, esp_y + 5.0, dht_x, dht_y - 0.4, C_DHT)
# DHT VCC al carril rojo arriba (3.3V), GND al azul
ax.plot([dht_x - 0.2, dht_x - 0.2], [dht_y - 0.4, pb_y + pb_h - 0.45],
        color=C_VCC, linewidth=1.2, alpha=0.85)
ax.plot([dht_x + 0.2, dht_x + 0.2], [dht_y - 0.4, pb_y + pb_h - 0.7],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 34 (MQ-6 DO)  — pin izq
cable(x_pin_izq, esp_y + 5.4, mq_x + 0.08, mq_y - 0.4, C_MQ)
# MQ-6 VCC: necesita 5V. Toma de carril (pero el carril rojo tiene 3.3V).
# En la practica: VCC del MQ va a VIN del ESP32 directamente (no por carril rojo).
# Lo dibujo como un cable rojo punteado al carril rojo + nota.
ax.plot([mq_x - 0.25, mq_x - 0.25], [mq_y - 0.4, pb_y + pb_h - 0.45],
        color=C_VCC, linewidth=1.2, alpha=0.85, linestyle='--')
ax.plot([mq_x - 0.08, mq_x - 0.08], [mq_y - 0.4, pb_y + pb_h - 0.7],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 27 (PIR OUT) — pin izq y=esp_y+3.0
cable(x_pin_izq, esp_y + 3.0, pir_x, pir_y - 0.4, C_PIR)
ax.plot([pir_x - 0.18, pir_x - 0.18], [pir_y - 0.4, pb_y + pb_h - 0.45],
        color=C_VCC, linewidth=1.2, alpha=0.85, linestyle='--')
ax.plot([pir_x + 0.18, pir_x + 0.18], [pir_y - 0.4, pb_y + pb_h - 0.7],
        color=C_GND, linewidth=1.2, alpha=0.85)

# GPIO 13 (Servo señal)
cable(x_pin_izq, esp_y + 1.8, srv_x + 0.05, srv_y + 0.2, C_SERVO)

# RC522: SDA=5, SCK=18, MOSI=23, MISO=19, RST=22 (todos pin der)
cable(x_pin_der + 0.13, esp_y + 3.8, rc_x - 0.42, rc_y - 0.5, C_SDA)   # SDA → GPIO5
cable(x_pin_der + 0.13, esp_y + 3.4, rc_x - 0.28, rc_y - 0.5, C_SCK)   # SCK → GPIO18
cable(x_pin_der + 0.13, esp_y + 1.0, rc_x - 0.14, rc_y - 0.5, C_MOSI)  # MOSI → GPIO23
cable(x_pin_der + 0.13, esp_y + 3.0, rc_x + 0.0, rc_y - 0.5, C_MISO)   # MISO → GPIO19
cable(x_pin_der + 0.13, esp_y + 1.4, rc_x + 0.42, rc_y - 0.5, C_RST)   # RST → GPIO22
# RC522 3.3V y GND
ax.plot([rc_x + 0.55, rc_x + 0.55], [rc_y - 0.5, pb_y + 0.7],
        color=C_VCC, linewidth=1.2, alpha=0.85)
ax.plot([rc_x + 0.28, rc_x + 0.28], [rc_y - 0.5, pb_y + 0.45],
        color=C_GND, linewidth=1.2, alpha=0.85)

# ===== Leyenda de cables =====
ley_x, ley_y = 0.4, 0.05
ax.add_patch(FancyBboxPatch((ley_x, ley_y), 15.2, 1.3,
                            boxstyle="round,pad=0.05",
                            facecolor='#fff8e6', edgecolor='#d4a373', linewidth=1))
ax.text(ley_x + 0.2, ley_y + 1.05, "LEYENDA DE COLORES DE CABLE:",
        fontsize=9, fontweight='bold', color='#7c4019')

leyenda = [
    (C_VCC, "+VCC (3.3V/5V)"),
    (C_GND, "GND"),
    (C_LED, "GPIO 2 → LED"),
    (C_BUZZ, "GPIO 15 → Buzzer"),
    (C_BOTON, "GPIO 14 → Botón"),
    (C_DHT, "GPIO 4 → DHT11 DATA"),
    (C_MQ, "GPIO 34 → MQ-6 DO"),
    (C_PIR, "GPIO 27 → PIR OUT"),
    (C_SERVO, "GPIO 13 → Servo señal"),
]
# Distribuir en 2 filas
col_w = 15.2 / 5
for i, (color, label) in enumerate(leyenda):
    row = i // 5
    col = i % 5
    cx = ley_x + 0.3 + col * col_w
    cy = ley_y + 0.7 - row * 0.3
    ax.plot([cx, cx + 0.3], [cy, cy], color=color, linewidth=2.5)
    ax.text(cx + 0.4, cy, label, fontsize=7, va='center')

# Linea adicional con SPI/RC522
ax.text(ley_x + 0.3, ley_y + 0.18,
        "RC522 (SPI 3.3V):  SDA=GPIO5  SCK=GPIO18  MOSI=GPIO23  MISO=GPIO19  RST=GPIO22    ⚠ NUNCA conectar a 5V",
        fontsize=7, color='#a64d6d', fontweight='bold')

# Aviso sobre servo y RC522
ax.text(8, 1.55,
        "⚠ El servo SG90 debe alimentarse con fuente externa 5V (GND común con ESP32). "
        "El RC522 es 3.3V — nunca 5V.",
        ha='center', fontsize=7, color='#b8500e', style='italic')

# ===== Guardar =====
out_path = os.path.expanduser(r"~\Downloads\diagrama_casa_inteligente_v2.pdf")
plt.savefig(out_path, format='pdf', bbox_inches='tight', dpi=200)
print(f"PDF guardado en: {out_path}")
