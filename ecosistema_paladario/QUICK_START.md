# 🚀 QUICK START - Paladario WiFi

## 📝 Pasos Rápidos

### 1. Configurar WiFi
Edita `src/wifi_config.h`:
```c
#define WIFI_SSID "TU_RED_WIFI"
#define WIFI_PASSWORD "TU_CONTRASEÑA"
```

### 2. Subir Código
```bash
pio run --target upload
pio device monitor
```

### 3. Obtener IP
Busca en el monitor serial:
```
I (2345) PALADARIO: IP obtenida: 192.168.1.150
```

### 4. Acceder desde Navegador
```
http://192.168.1.150
```

## 🏠 Home Assistant (Opcional)

### 1. Instalar Mosquitto Broker en HA
- Settings → Add-ons → Mosquitto broker → Install

### 2. Configurar MQTT en `src/wifi_config.h`
```c
#define MQTT_BROKER "192.168.1.100"  // IP de Home Assistant
#define MQTT_USER "homeassistant"
#define MQTT_PASS "tu_password"
```

### 3. Listo!
Los dispositivos aparecerán automáticamente en Home Assistant.

## 📊 Endpoints API

- `http://IP/` → Panel web
- `http://IP/api/status` → JSON con todos los datos
- `http://IP/api/lluvia/toggle` → Toggle bomba lluvia
- `http://IP/api/cascada/toggle` → Toggle bomba cascada

## 🔧 Conexiones Físicas

```
DHT11:    VCC→3V3,  DATA→GPIO4,   GND→GND
Relé 1:   VCC→VIN,  IN→GPIO25,    GND→GND  (Lluvia)
Relé 2:   VCC→VIN,  IN→GPIO26,    GND→GND  (Cascada)
```

¡Listo para usar! 🎉
