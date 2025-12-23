# 📋 Documentación del Sistema de Control del Paladario

## 🎯 Funcionalidades Implementadas

### 1. **Monitoreo de Temperatura y Humedad**
- Sensor: DHT11 en GPIO 4
- Frecuencia de lectura: Cada 5 segundos
- Muestra en consola: Temperatura en °C y Humedad en %

### 2. **Control Automático de Bomba de Lluvia**
- Pin de control: GPIO 25
- Lógica automática:
  - Si humedad < 60% → Activa bomba de lluvia
  - Si humedad > 80% → Desactiva bomba de lluvia
- Se puede controlar manualmente modificando el código

### 3. **Control de Bomba de Cascada**
- Pin de control: GPIO 26
- Ciclo automático:
  - 30 minutos ENCENDIDA
  - 30 minutos APAGADA
  - Ciclo infinito

### 4. **Monitoreo en Tiempo Real**
- Cada 10 segundos muestra en el monitor serial:
  - Temperatura actual
  - Humedad actual
  - Estado de bomba lluvia (ON/OFF)
  - Estado de bomba cascada (ON/OFF)

---

## 🔧 Configuración Actual

```c
// Pines definidos
GPIO 4  → Sensor DHT11 (DATA)
GPIO 25 → Relé bomba lluvia (señal)
GPIO 26 → Relé bomba cascada (señal)

// Umbrales de humedad
Humedad baja: < 60%  → Activa lluvia
Humedad alta: > 80%  → Desactiva lluvia

// Tiempos de ciclo cascada
30 minutos ON
30 minutos OFF
```

---

## ⚙️ Personalización

### Cambiar umbrales de humedad:
Edita en `src/main.c`, función `task_leer_sensor()`:
```c
if (humedad < 60.0 && !bomba_lluvia_activa) {  // Cambiar 60.0
    control_bomba_lluvia(true);
} else if (humedad > 80.0 && bomba_lluvia_activa) {  // Cambiar 80.0
    control_bomba_lluvia(false);
}
```

### Cambiar ciclo de cascada:
Edita en `src/main.c`, función `task_ciclo_cascada()`:
```c
vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));  // 30 minutos = 30*60*1000 ms
// Cambiar el número 30 por los minutos deseados
```

### Cambiar frecuencia de lectura del sensor:
Edita en `src/main.c`, función `task_leer_sensor()`:
```c
vTaskDelay(pdMS_TO_TICKS(5000));  // 5000 ms = 5 segundos
// Cambiar 5000 por los milisegundos deseados
```

---

## 📊 Estructura del Código

```
src/
├── main.c       → Programa principal
├── dht11.c      → Driver del sensor DHT11
└── dht11.h      → Declaraciones del driver DHT11

Funciones principales en main.c:
├── config_gpio()           → Configura pines como entrada/salida
├── control_bomba_lluvia()  → Enciende/apaga bomba lluvia
├── control_bomba_cascada() → Enciende/apaga bomba cascada
├── task_leer_sensor()      → Lee DHT11 cada 5 seg
├── task_ciclo_cascada()    → Ciclo automático cascada
├── task_mostrar_estado()   → Muestra estado cada 10 seg
└── app_main()              → Inicializa todo
```

---

## 🚀 Cómo Usar

### 1. Compilar:
```bash
cd "c:\Users\diego\OneDrive\Documentos\PlatformIO\Projects\ecosistema_paladario"
pio run
```

### 2. Subir al ESP32:
```bash
pio run --target upload
```

### 3. Ver monitor serial:
```bash
pio device monitor
```

### 4. Salida esperada en el monitor:
```
I (308) PALADARIO: Iniciando sistema de control de Paladario
I (308) PALADARIO: GPIOs configurados correctamente
I (308) PALADARIO: Sistema iniciado correctamente
I (308) PALADARIO: Iniciando lectura de sensor DHT11...
I (308) PALADARIO: Iniciando ciclo de cascada...
I (308) PALADARIO: Bomba cascada: ENCENDIDA
I (2308) PALADARIO: Temperatura: 25.0°C, Humedad: 55.0%
I (2308) PALADARIO: Humedad baja, activando efecto lluvia
I (2308) PALADARIO: Bomba lluvia: ENCENDIDA
I (10308) PALADARIO: === ESTADO PALADARIO ===
I (10308) PALADARIO: Temperatura: 25.0°C
I (10308) PALADARIO: Humedad: 55.0%
I (10308) PALADARIO: Bomba lluvia: ON
I (10308) PALADARIO: Bomba cascada: ON
I (10308) PALADARIO: ========================
```

---

## 🔌 Conexiones Físicas

```
DHT11 Módulo:
  VCC/+ → 3V3 del ESP32
  DATA  → GPIO 4
  GND/- → GND del ESP32

Relé Bomba Lluvia:
  VCC → VIN del ESP32 (5V)
  IN  → GPIO 25
  GND → GND del ESP32

Relé Bomba Cascada:
  VCC → VIN del ESP32 (5V)
  IN  → GPIO 26
  GND → GND del ESP32
```

---

## ⚠️ Notas Importantes

1. **Sensibilidad del DHT11:**
   - No leerlo más rápido que cada 2 segundos
   - Puede dar errores ocasionales (es normal)
   - El código reintenta automáticamente en el siguiente ciclo

2. **Control de relés:**
   - Los relés se activan con señal HIGH (3.3V)
   - Asegúrate que sean módulos con optoacoplador
   - Verifica que soporten la carga de tus bombas

3. **Alimentación:**
   - ESP32 consume ~500mA máximo
   - Cada relé consume ~70mA
   - DHT11 consume ~2.5mA
   - Total: ~650mA (usar fuente de 1A mínimo)

4. **Protección:**
   - Los relés protegen el ESP32 de la carga de las bombas
   - No conectar bombas directamente al ESP32
   - Siempre usar relés o transistores

---

## 🐛 Solución de Problemas

### Error al leer sensor DHT11:
- Verificar conexiones (VCC, DATA, GND)
- Verificar que el pin DATA esté en GPIO 4
- Sensor puede tardar hasta 2 segundos en responder

### Bombas no se activan:
- Verificar que los relés estén alimentados (VIN = 5V)
- Verificar conexiones de señal (GPIO 25 y 26)
- Probar con LED en lugar de relé para depurar

### ESP32 no compila/sube:
- Verificar que esté conectado el cable USB
- Presionar botón BOOT al subir código
- Verificar puerto COM en Device Manager

---

## 📝 Próximas Mejoras Opcionales

1. **Control WiFi:**
   - Añadir servidor web para control remoto
   - Ver datos en tiempo real desde navegador

2. **Pantalla OLED:**
   - Mostrar temperatura y humedad en pantalla
   - Ver estado de bombas sin PC

3. **Registro de datos:**
   - Guardar histórico en tarjeta SD
   - Gráficas de temperatura/humedad

4. **Sensor de nivel de agua:**
   - Detectar si hay suficiente agua
   - Apagar bombas si el nivel es bajo

5. **RTC (Reloj de Tiempo Real):**
   - Programar horarios específicos
   - Ciclos basados en hora del día
