# 🔌 ESQUEMA DE CONEXIONES - DHT11 + ESP32

## 📋 Módulo DHT11 de 3 Pines

```
Vista del módulo DHT11 (mirando de frente al sensor azul):

┌─────────────────────────┐
│   Módulo DHT11 PCB      │
│   ┌───────────────┐     │
│   │   DHT11       │     │  ← Sensor azul con rejilla
│   │  (rejilla)    │     │
│   └───────────────┘     │
│                         │
└─────────────────────────┘
   │      │      │
   │      │      └─── [-] GND    → Color cable: NEGRO o MARRÓN
   │      └────────── [OUT] DATA → Color cable: AMARILLO o VERDE
   └───────────────── [+] VCC    → Color cable: ROJO

Serigrafía común en el módulo:
┌───┬─────┬─────┐
│ + │ OUT │  -  │
└───┴─────┴─────┘
 o también:
┌─────┬──────┬─────┐
│ VCC │ DATA │ GND │
└─────┴──────┴─────┘
```

---

## 🔧 ESP32 DevKit V4 - Vista de Pines

```
ESP32 DevKit V4 (30 pines)
Puerto USB hacia ARRIBA ↑

Lado IZQUIERDO          Lado DERECHO
════════════════        ════════════════
3V3  ←─ VCC DHT11      GND
EN                      GPIO 23
GPIO 36 (VP)           GPIO 22
GPIO 39 (VN)           GPIO 1 (TX0)
GPIO 34                 GPIO 3 (RX0)
GPIO 35                 GPIO 21
GPIO 32                 GPIO 19
GPIO 33 ← CALEFACCIÓN  GPIO 18
GPIO 25 ← BOMBA LLUVIA GPIO 5
GPIO 26 ← BOMBA CASCA. GPIO 17
GPIO 27 ← VENTILADOR   GPIO 16
GPIO 14                 GPIO 4  ←─ DATA DHT11 ⭐
GPIO 12                 GPIO 0
GPIO 13                 GPIO 2  ←─ (prueba alternativa)
GND  ←─ GND DHT11      GPIO 15
VIN                     GND
```

---

## ✅ CONEXIÓN CORRECTA

```
┌──────────────────────┐              ┌──────────────────────┐
│  MÓDULO DHT11        │              │    ESP32 DevKit V4   │
│                      │              │                      │
│  ┌────────────┐      │              │                      │
│  │  DHT11     │      │              │      [USB]           │
│  │ (sensor)   │      │              │       ___            │
│  └────────────┘      │              │      |   |           │
│                      │              │                      │
│   [+]  [OUT]  [-]    │              │                      │
│    │     │     │     │              │                      │
└────┼─────┼─────┼─────┘              └──────────────────────┘
     │     │     │                          │     │     │
     │     │     └──────────────────────────┤     │     │
     │     │         CABLE NEGRO/MARRÓN     │     │     │
     │     │                               GND    │     │
     │     │                                      │     │
     │     └────────────────────────────────────┐ │     │
     │              CABLE AMARILLO/VERDE        │ │     │
     │                                       GPIO 4     │
     │                                          │ │     │
     └──────────────────────────────────────────┼─┤     │
                  CABLE ROJO                    │ │     │
                                               3V3 │     │
                                                  │     │
                                                 GND   3V3
```

---

## 🔍 VERIFICACIÓN DE CONEXIONES

### ✓ Lista de verificación:

1. **Cable ROJO (VCC)**
   - Desde pin [+] del DHT11
   - Hasta pin **3V3** del ESP32 (lado izquierdo, arriba del todo)
   - ⚠️ NO usar 5V, usar 3.3V

2. **Cable NEGRO/MARRÓN (GND)**
   - Desde pin [-] del DHT11
   - Hasta pin **GND** del ESP32 (hay varios, usa el del lado izquierdo cerca del 3V3)

3. **Cable AMARILLO/VERDE (DATA)**
   - Desde pin [OUT] o [DATA] del DHT11
   - Hasta pin **GPIO 4** del ESP32 (lado derecho, posición 12 desde abajo)
   - ⚠️ Este es el cable que está dando problemas

---

## ⚠️ ERRORES COMUNES

### ❌ Error 1: Pines invertidos
```
INCORRECTO:
DHT11 [-] → ESP32 3V3   ✗ MAL!
DHT11 [+] → ESP32 GND   ✗ MAL!
```

### ❌ Error 2: DATA conectado a GND
```
Tu problema actual:
DHT11 [OUT] → hace corto con GND
Resultado: Pin siempre en LOW
```

### ❌ Error 3: VCC sin conectar
```
DHT11 [+] → sin conexión
Resultado: Pin siempre en HIGH, no responde
```

---

## 🔧 DIAGNÓSTICO CON MULTÍMETRO

### Con el DHT11 conectado al ESP32 encendido:

1. **Medir voltaje VCC:**
   ```
   Multímetro en modo DC Voltaje
   Punta ROJA  → Pin [+] del DHT11
   Punta NEGRA → Pin [-] del DHT11
   
   Debería leer: 3.3V ± 0.1V
   Si lee 0V: VCC no conectado
   ```

2. **Medir voltaje DATA en reposo:**
   ```
   Multímetro en modo DC Voltaje
   Punta ROJA  → Pin [OUT] del DHT11
   Punta NEGRA → Pin [-] del DHT11
   
   Debería leer: 3.3V (por pull-up)
   Si lee 0V: DATA en corto con GND ← TU PROBLEMA
   ```

3. **Verificar continuidad (con ESP32 APAGADO):**
   ```
   Multímetro en modo CONTINUIDAD (beep)
   
   TEST 1:
   Punta 1 → Pin [OUT] del DHT11
   Punta 2 → Pin [-] del DHT11
   
   NO debe hacer BEEP (no debe haber continuidad)
   Si hace BEEP: Hay cortocircuito ← TU PROBLEMA
   
   TEST 2:
   Punta 1 → Pin [+] del DHT11
   Punta 2 → Pin [-] del DHT11
   
   NO debe hacer BEEP
   Si hace BEEP: Cortocircuito VCC-GND
   ```

---

## 🔨 SOLUCIÓN AL PROBLEMA ACTUAL

### Tu sensor muestra: **Pin en LOW permanente**

**Causa:** El pin DATA está en cortocircuito con GND

**Posibles soluciones:**

1. **Revisar soldadura en el módulo DHT11:**
   - ¿Hay un puente de estaño entre pines OUT y GND?
   - Dessoldar y volver a soldar con cuidado

2. **Verificar el cable DATA:**
   - ¿El aislante está dañado?
   - ¿Toca algún otro cable o componente?

3. **Probar con cables diferentes:**
   - Puede que el cable tenga daño interno

4. **Reemplazar el sensor DHT11:**
   - El sensor puede estar dañado internamente

---

## 📸 FOTO DE REFERENCIA DE PINES

```
Módulo DHT11 típico visto desde arriba:

     ╔════════════════╗
     ║  DHT11 MODULE  ║
     ║  ╔══════════╗  ║
     ║  ║ ░░░░░░░░ ║  ║  ← Rejilla del sensor
     ║  ║ ░░DHT11░ ║  ║
     ║  ║ ░░░░░░░░ ║  ║
     ║  ╚══════════╝  ║
     ║                ║
     ║  +   OUT   -   ║  ← Serigrafía
     ╚════════════════╝
        │    │    │
      ROJO AMARI NEGRO
      3V3  GPIO4  GND
```

---

## 🎯 CÓDIGO ACTUAL

El código actual está configurado para **GPIO 2** (prueba).

Para volver a GPIO 4 (correcto según hardware), cambiar en `main.c`:
```c
#define DHT_GPIO 4  // Pin correcto según documentación
```

---

## 📞 PRÓXIMOS PASOS

1. ✓ Apaga el ESP32
2. ✓ Desconecta el cable DATA (amarillo)
3. ✓ Mide con multímetro:
   - Continuidad entre DATA y GND del módulo DHT11
   - Voltaje entre VCC y GND (debería ser 3.3V con ESP32 encendido)
4. ✓ Si hay cortocircuito, revisa la soldadura del módulo
5. ✓ Reconecta y prueba de nuevo

