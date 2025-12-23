# 🌐 Guía de Configuración WiFi y Home Assistant

## 📡 Configuración WiFi

### 1. Editar credenciales WiFi

Abre el archivo `src/wifi_config.h` y modifica:

```c
#define WIFI_SSID ""        // Nombre de tu red WiFi
#define WIFI_PASSWORD ""  // Contraseña de tu WiFi
```

### 2. Compilar y subir

```bash
cd "c:\Users\diego\OneDrive\Documentos\PlatformIO\Projects\ecosistema_paladario"
pio run --target upload
pio device monitor
```

### 3. Obtener la IP del ESP32

En el monitor serial verás algo como:
```
I (2345) PALADARIO: IP obtenida: 192.168.1.150
```

Anota esta IP para acceder al panel web.

---

## 🖥️ Acceso al Panel Web

### Desde cualquier navegador en tu red local:

```
http://192.168.1.150
```
(Usa la IP que obtuviste en el paso anterior)

### Características del Panel Web:

✅ **Ver en tiempo real:**
- Temperatura actual
- Humedad actual
- Estado de bombas (ON/OFF)

✅ **Controlar:**
- Encender/apagar bomba de lluvia con un click
- Encender/apagar bomba de cascada con un click

✅ **Auto-actualización:**
- La página se recarga cada 10 segundos automáticamente

---

## 🏠 Integración con Home Assistant

### Prerequisitos:

1. **Tener Home Assistant instalado y funcionando**
2. **MQTT Broker configurado** (Mosquitto addon recomendado)

### Paso 1: Configurar MQTT en Home Assistant

#### Instalar Mosquitto Broker:

1. Ve a `Settings` → `Add-ons` → `Add-on Store`
2. Busca **"Mosquitto broker"**
3. Click en `Install`
4. Una vez instalado, ve a la pestaña `Configuration`
5. Habilita estas opciones:
   ```yaml
   logins:
     - username: homeassistant
       password: tu_password_mqtt
   ```
6. Click en `Save` y luego `Start`

#### Configurar integración MQTT:

1. Ve a `Settings` → `Devices & Services`
2. Click en `Add Integration`
3. Busca **"MQTT"**
4. Configura:
   - Broker: `localhost` (o la IP de tu Home Assistant)
   - Port: `1883`
   - Username: `homeassistant`
   - Password: `tu_password_mqtt`

### Paso 2: Configurar el ESP32

Edita `src/wifi_config.h`:

```c
#define MQTT_BROKER "192.168.1.100"    // IP de tu Home Assistant
#define MQTT_PORT 1883
#define MQTT_USER "homeassistant"      // Usuario MQTT
#define MQTT_PASS "tu_password_mqtt"   // Contraseña MQTT
```

### Paso 3: Compilar y subir

```bash
pio run --target upload
```

### Paso 4: Verificar en Home Assistant

Después de unos segundos, deberías ver automáticamente en Home Assistant:

**Dispositivos detectados:**
- 🌡️ **Paladario Temperatura** (sensor)
- 💧 **Paladario Humedad** (sensor)
- 💦 **Bomba Lluvia** (switch)
- 🌊 **Bomba Cascada** (switch)

**¿Dónde encontrarlos?**
1. Ve a `Settings` → `Devices & Services`
2. Click en `MQTT`
3. Deberías ver un dispositivo llamado **"Paladario"**

---

## 🎨 Crear Tarjeta en Home Assistant

### Dashboard personalizado:

```yaml
type: vertical-stack
cards:
  - type: entities
    title: 🌿 Paladario
    entities:
      - entity: sensor.paladario_temperatura
        name: Temperatura
        icon: mdi:thermometer
      - entity: sensor.paladario_humedad
        name: Humedad
        icon: mdi:water-percent
      - entity: switch.bomba_lluvia
        name: Bomba Lluvia
        icon: mdi:water
      - entity: switch.bomba_cascada
        name: Bomba Cascada
        icon: mdi:waterfall
  - type: history-graph
    title: Historial
    entities:
      - sensor.paladario_temperatura
      - sensor.paladario_humedad
    hours_to_show: 24
```

**Cómo agregar:**
1. Ve a tu Dashboard
2. Click en los 3 puntos (arriba derecha) → `Edit Dashboard`
3. Click en `+ Add Card`
4. Selecciona `Manual` abajo
5. Pega el código YAML anterior
6. Click en `Save`

---

## 🔧 API REST Endpoints

### Obtener estado completo:

```bash
GET http://192.168.1.150/api/status
```

**Respuesta JSON:**
```json
{
  "temperatura": 25.5,
  "humedad": 68.0,
  "bomba_lluvia": true,
  "bomba_cascada": false
}
```

### Toggle Bomba Lluvia:

```bash
GET http://192.168.1.150/api/lluvia/toggle
```

### Toggle Bomba Cascada:

```bash
GET http://192.168.1.150/api/cascada/toggle
```

---

## 📊 Tópicos MQTT

### Sensores (lectura):

```
paladario/sensor/temperatura/state    → "25.5"
paladario/sensor/humedad/state        → "68.0"
```

### Switches Estado (lectura):

```
paladario/switch/bomba_lluvia/state   → "ON" o "OFF"
paladario/switch/bomba_cascada/state  → "ON" o "OFF"
```

### Switches Comandos (escritura):

```
paladario/switch/bomba_lluvia/set     ← "ON" o "OFF"
paladario/switch/bomba_cascada/set    ← "ON" o "OFF"
```

---

## 🎯 Automatizaciones en Home Assistant

### Ejemplo 1: Notificación de temperatura alta

```yaml
automation:
  - alias: "Alerta Temperatura Alta Paladario"
    trigger:
      - platform: numeric_state
        entity_id: sensor.paladario_temperatura
        above: 30
    action:
      - service: notify.mobile_app
        data:
          title: "⚠️ Temperatura Alta"
          message: "El paladario tiene {{ states('sensor.paladario_temperatura') }}°C"
```

### Ejemplo 2: Encender lluvia automáticamente

```yaml
automation:
  - alias: "Lluvia Automática Paladario"
    trigger:
      - platform: numeric_state
        entity_id: sensor.paladario_humedad
        below: 50
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.bomba_lluvia
```

### Ejemplo 3: Cascada solo de día

```yaml
automation:
  - alias: "Cascada Solo de Día"
    trigger:
      - platform: sun
        event: sunrise
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.bomba_cascada
  
  - alias: "Apagar Cascada de Noche"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.bomba_cascada
```

---

## 🔍 Solución de Problemas

### No aparece en Home Assistant:

1. **Verificar MQTT:**
   - Ve a `Settings` → `Devices & Services` → `MQTT`
   - Click en `Configure` → `Listen to a topic`
   - Escribe: `paladario/#`
   - Deberías ver mensajes llegando

2. **Verificar logs del ESP32:**
   ```bash
   pio device monitor
   ```
   Busca: `MQTT conectado` y `Descubrimiento MQTT enviado`

3. **Reiniciar Home Assistant:**
   - `Settings` → `System` → `Restart`

### No puedo acceder al panel web:

1. Verificar que estés en la misma red WiFi
2. Hacer ping a la IP del ESP32
3. Verificar firewall/router
4. Revisar logs en el monitor serial

### MQTT no conecta:

1. Verificar IP correcta del broker
2. Verificar usuario y contraseña
3. Verificar que Mosquitto esté corriendo
4. Revisar puerto 1883 abierto

---

## 📱 Aplicación Móvil Home Assistant

Con la app de Home Assistant en tu móvil puedes:

✅ Ver temperatura y humedad en tiempo real
✅ Controlar las bombas desde cualquier lugar
✅ Recibir notificaciones
✅ Widgets en pantalla de inicio
✅ Control por voz con Google Assistant/Alexa

**Descargar:**
- iOS: App Store → "Home Assistant"
- Android: Play Store → "Home Assistant"

---

## 🎉 ¡Todo Listo!

Ahora tienes:

✅ Panel web accesible desde cualquier navegador
✅ Integración completa con Home Assistant
✅ Control desde móvil
✅ Automatizaciones inteligentes
✅ Historial de datos
✅ Notificaciones

**Próximos pasos opcionales:**
- Agregar más sensores (luz, pH, etc.)
- Crear escenas personalizadas
- Integrar con Google Home/Alexa
- Agregar cámara para ver el paladario
