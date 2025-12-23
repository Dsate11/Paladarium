# 🔌 Mapa de Pines ESP32 DevKit V4

## Ubicación Física de los Pines

```
ESP32 DevKit V4 - Vista Superior (30 pines)

Lado Izquierdo              Lado Derecho
================            ================
3V3                         GND
EN                          GPIO 23
GPIO 36 (VP)               GPIO 22
GPIO 39 (VN)               GPIO 1 (TX0)
GPIO 34                     GPIO 3 (RX0)
GPIO 35                     GPIO 21
GPIO 32                     GPIO 19
GPIO 33                     GPIO 18
GPIO 25                     GPIO 5
GPIO 26                     GPIO 17
GPIO 27                     GPIO 16
GPIO 14                     GPIO 4    ← ⭐ AQUÍ ESTÁ GPIO 4
GPIO 12                     GPIO 0
GPIO 13                     GPIO 2
GND                         GPIO 15
VIN                         GND
```

## 📍 Pines Usados en tu Proyecto

| Pin Físico | GPIO | Función en el Proyecto | Conexión |
|------------|------|------------------------|----------|
| Derecha #12| GPIO 4 | Sensor DHT11 (DATA) | Cable de señal del DHT11 |
| Izquierda #9| GPIO 25 | Control Bomba Lluvia | Señal al relé 1 (IN) |
| Izquierda #10| GPIO 26 | Control Bomba Cascada | Señal al relé 2 (IN) |

## 🎯 Cómo Ubicar GPIO 4

**Método 1 - Contando desde abajo:**
1. Coloca el ESP32 con el puerto USB hacia arriba
2. En el **lado DERECHO**, cuenta desde abajo
3. GPIO 4 está en la posición **#12** desde abajo (o #4 desde arriba)

**Método 2 - Busca la serigrafía:**
- En la placa debería estar impreso "4" o "IO4" junto al pin
- Está entre GPIO 16 y GPIO 2

**Método 3 - Referencia visual:**
```
         [USB]
          ___
         |   |
         |   |
   ┌─────────────┐
   │  ESP32      │
   │  DevKit V4  │
   │             │
   └─────────────┘
    │ │ │   │ │ │
    │ │ │   ↓ │ │  ← GPIO 4 (aprox. mitad inferior derecha)
    │ │ │  G │ │
    │ │ │  P │ │
    │ │ │  I │ │
    │ │ │  O │ │
    └─┴─┴──4─┴─┘
```

## ⚠️ Importante

- **GPIO 4** es un pin seguro (no afecta el arranque del ESP32)
- Soporta funciones: Input, Output, PWM, Touch
- Voltaje lógico: **3.3V**
- Corriente máxima recomendada: **20mA**

## 🔗 Conexión Completa

```
DHT11 Módulo                    ESP32 DevKit V4
─────────────                   ───────────────
[+] ─────────────────────────→ 3V3 (pin superior izquierdo)
[OUT/DATA] ──────────────────→ GPIO 4 (lado derecho, pos. 12)
[-] ─────────────────────────→ GND (cualquier pin GND)
```

## 📱 Verificación con Código

Si no estás seguro del pin, puedes probarlo. El código ya tiene definido:
```c
#define DHT_GPIO 4    // Pin GPIO 4
```

Si conectaste el DHT11 a otro GPIO, solo cambia el número en esta línea.
