# 📌 PINOUT DETALLADO - Sistema Paladario ESP32

## 🔌 Conexiones del Sistema

### **Sensor DHT11 (Temperatura y Humedad)**

**Módulo de 3 pines (con resistencia pull-up integrada):**

**Cómo identificar los pines:**

1. **Por serigrafía en la PCB** (lo más común):
   - Busca las letras impresas en la tarjeta: `+`, `OUT`/`S`/`DATA`, `-`/`GND`
   - O pueden estar como: `VCC`, `DATA`, `GND`

2. **Por posición física** (vista desde el frente del sensor):
   ```
   ┌─────────────┐
   │   DHT11     │  ← Sensor azul con rejilla
   │  (rejilla)  │
   └─────────────┘
   │   │   │   │
   │   │   │   └─ Pin 4: GND (puede estar NC - no conectado)
   │   │   └───── Pin 3: NC (No Conectar) o no existe
   │   └───────── Pin 2: DATA (señal)
   └───────────── Pin 1: VCC (+)
   ```

3. **En módulos de 3 pines** (tarjeta verde/azul):
   ```
   Tarjeta PCB vista de frente:
   
   [+]  [OUT]  [-]     ← Serigrafía común
    │     │     │
   VCC  DATA  GND
   ```
   
   O también puede ser:
   ```
   [S]  [VCC]  [GND]   ← Otra disposición común
    │     │      │
   DATA  VCC   GND
   ```

**Conexión segura al ESP32:**
```
Módulo DHT11   →    ESP32
---------------------------------
+ / VCC        →    3.3V o 5V
OUT / S / DATA →    GPIO 4
- / GND        →    GND
```

**Nota:** Este módulo ya incluye la resistencia pull-up en la tarjeta, **NO necesitas agregar resistencia externa**

**🔍 Si no hay marcas visibles:**
- Con un multímetro, mide continuidad entre los pines del módulo y el sensor DHT11
- El pin GND del módulo debe tener continuidad con el pin 4 del sensor DHT11
- El pin VCC del módulo debe tener continuidad con el pin 1 del sensor DHT11

**Sensor DHT11 de 4 pines (sin módulo):**
```
DHT11 Sensor   →    Conexión
---------------------------------
VCC (Pin 1)    →    3.3V o 5V
DATA (Pin 2)   →    GPIO 4 + resistencia 10kΩ a VCC
NC (Pin 3)     →    No conectar
GND (Pin 4)    →    GND
```

---

### **Relé Bomba de Lluvia**
```
Módulo Relé    →    ESP32
---------------------------------
VCC            →    5V (VIN)
GND            →    GND
IN (Signal)    →    GPIO 25
```
**Conexión de la bomba:**
- COM (Común) → Cable de fase (220V) o positivo (12V)
- NO (Normalmente Abierto) → Bomba
- La bomba completa el circuito con Neutro (220V) o GND (12V)

---

### **Relé Bomba de Cascada**
```
Módulo Relé    →    ESP32
---------------------------------
VCC            →    5V (VIN)
GND            →    GND
IN (Signal)    →    GPIO 26
```
**Conexión de la bomba:**
- COM (Común) → Cable de fase (220V) o positivo (12V)
- NO (Normalmente Abierto) → Bomba
- La bomba completa el circuito con Neutro (220V) o GND (12V)

---

## 🔧 Resumen de Pines ESP32 Utilizados

| Pin GPIO | Función                  | Tipo    | Descripción                      |
|----------|--------------------------|---------|----------------------------------|
| GPIO 4   | Sensor DHT11 DATA        | INPUT   | Lectura temperatura/humedad      |
| GPIO 25  | Control Bomba Lluvia     | OUTPUT  | Activa/desactiva relé lluvia     |
| GPIO 26  | Control Bomba Cascada    | OUTPUT  | Activa/desactiva relé cascada    |
| 3.3V     | Alimentación sensores    | POWER   | Para DHT11                       |
| 5V (VIN) | Alimentación relés       | POWER   | Para módulos relé                |
| GND      | Tierra común             | GROUND  | Compartida todos los componentes |

---

## 🎨 Diagrama de Conexión Simplificado

```
                    ┌─────────────────┐
                    │     ESP32       │
                    │  DevKit V4      │
                    └─────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
    GPIO 4             GPIO 25            GPIO 26
        │                   │                   │
   ┌────▼────┐         ┌────▼────┐        ┌────▼────┐
   │ DHT11   │         │ RELÉ 1  │        │ RELÉ 2  │
   │ Temp/Hum│         │ LLUVIA  │        │ CASCADA │
   └─────────┘         └────┬────┘        └────┬────┘
                            │                  │
                       ┌────▼────┐        ┌────▼────┐
                       │ BOMBA   │        │ BOMBA   │
                       │ LLUVIA  │        │ CASCADA │
                       └─────────┘        └─────────┘
```

---

## ⚠️ ADVERTENCIAS DE SEGURIDAD

### **Si usas 220V AC:**
1. ⚡ **PELIGRO:** Desconecta siempre la alimentación antes de manipular
2. Usa módulos relés con optoacoplador para aislamiento
3. Verifica que los relés soporten la corriente de tus bombas
4. Instala en caja eléctrica adecuada con protección IP65+ para humedad
5. Considera usar un interruptor diferencial (GFCI/RCD)

### **Si usas 12V DC:**
1. Mucho más seguro para ambientes húmedos
2. Usa fuente de alimentación de capacidad suficiente
3. Verifica polaridad correcta en las bombas
4. Los relés también deben soportar la corriente DC

---

## 🔋 Alimentación del Sistema

**Opción 1 - USB:**
- ESP32 alimentado por cable USB (5V)
- Limitado a corrientes bajas en los GPIOs
- Los relés necesitan fuente externa de 5V

**Opción 2 - Fuente Externa:**
- Alimentar VIN con 5-12V DC regulados
- Permite mayor corriente para módulos adicionales
- Compartir GND común entre ESP32 y fuente

---

## 📝 Notas Adicionales

- Los GPIOs del ESP32 entregan **3.3V lógico**
- Corriente máxima por pin: **40mA** (recomendado: 20mA)
- Los módulos relé suelen necesitar **5V** y ~70-80mA
- El DHT11 consume aproximadamente **2.5mA** en standby
- Pines GPIO 25 y 26 son seguros para usar (no tienen funciones especiales críticas)
- GPIO 4 también es seguro (no afecta el boot del ESP32)

---

## 🛠️ Lista de Materiales

| Cantidad | Componente                           | Especificaciones           |
|----------|--------------------------------------|----------------------------|
| 1        | ESP32 DevKit V4                      | 30 pines                   |
| 1        | Módulo DHT11                         | Temp/Humedad, 3 pines      |
| 2        | Módulo Relé 1 canal                  | 5V, optoacoplado           |
| 2        | Bomba de agua sumergible             | 12V o 220V según diseño    |
| 1        | Fuente de alimentación               | 5V/2A o 12V/2A             |
| -        | Cables dupont/jumper                 | Macho-hembra               |
| -        | Protoboard o PCB                     | Para montaje               |

