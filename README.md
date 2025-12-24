# 🌿 Paladarium - Sistema de Ecosistema Automatizado

Un proyecto de automatización para paladarium/terrario con control de clima, iluminación y monitoreo integrado con Home Assistant.

## 📋 Descripción

Este proyecto implementa un sistema completo de control y monitoreo para un paladarium, utilizando ESP32 y sensores diversos para mantener condiciones óptimas para plantas y animales. Incluye un avanzado sistema de iluminación LED con simulación de ciclos naturales (amanecer, día, atardecer, noche) y efectos atmosféricos (tormentas). La integración con Home Assistant permite el control remoto y la visualización de datos en tiempo real.

## 🌡️ Sistema de Control de Clima

El paladarium cuenta con un sistema automatizado de control climático que gestiona temperatura, humedad y efectos de agua mediante sensores y actuadores conectados a un ESP32.

### Componentes del Sistema
- **Sensor DHT11**: Monitoreo de temperatura y humedad ambiental
- **Bombas de Agua**: 
  - Bomba de lluvia para simulación de precipitación
  - Bomba de cascada para circulación de agua
- **Relés**: Control de encendido/apagado de bombas y otros dispositivos

### Documentación Técnica Detallada
Para información completa sobre conexiones, configuración y diagramas de cableado, consulta la documentación en la carpeta `ecosistema_paladario/`:

- 📌 [**ESP32_PINOUT.md**](ecosistema_paladario/ESP32_PINOUT.md) - Asignación completa de pines del ESP32
- 📌 [**PINOUT.md**](ecosistema_paladario/PINOUT.md) - Diagrama general de conexiones
- 📌 [**CONEXIONES_DHT11.md**](ecosistema_paladario/CONEXIONES_DHT11.md) - Guía específica de conexión del sensor DHT11
- 📌 [**README_SISTEMA.md**](ecosistema_paladario/README_SISTEMA.md) - Descripción completa del sistema
- 📌 [**CONFIGURACION_WIFI_HA.md**](ecosistema_paladario/CONFIGURACION_WIFI_HA.md) - Configuración de WiFi y Home Assistant

## 💡 Sistema de Iluminación

El paladarium cuenta con un sistema avanzado de iluminación LED controlado por ESP32, capaz de simular ciclos naturales de luz y efectos atmosféricos.

### Componentes de Hardware

#### Canales LED (Control PWM)
- **Canal Blanco 1** (Pin GPIO 5): Iluminación principal diurna
- **Canal Naranja** (Pin GPIO 18): Tonos cálidos para amanecer/atardecer
- **Canal Azul** (Pin GPIO 19): Iluminación nocturna y efectos de tormenta
- **Canal Blanco 2** (Pin GPIO 22): Iluminación secundaria diurna

#### Especificaciones Técnicas
- **Controlador**: ESP32 (AZ-Delivery DevKit v4)
- **Frecuencia PWM**: 20 kHz (sin parpadeo visible)
- **Resolución**: 8 bits (0-255 niveles de intensidad)
- **Botón de control**: GPIO 13 (con pull-up interno)

### Modos de Iluminación

El sistema implementa 5 modos de iluminación que simulan condiciones naturales:

#### 🌞 Modo Día
- **Descripción**: Brillo máximo para fotosíntesis óptima
- **Configuración**: 
  - Blanco 1: 255 (100%)
  - Naranja: 255 (100%)
  - Azul: 0
  - Blanco 2: 255 (100%)
- **Uso**: Período de máxima actividad de plantas

#### 🌅 Modo Amanecer
- **Descripción**: Transición gradual de oscuridad a luz diurna
- **Duración**: Configurable (por defecto 15 minutos)
- **Efecto**: Rampa lineal desde 0% hasta valores de modo día
- **Uso**: Simulación realista del amanecer natural
- **Automatización**: Se activa automáticamente al amanecer si está configurado

#### 🌃 Modo Noche
- **Descripción**: Iluminación mínima para descanso
- **Configuración**:
  - Blanco 1: 0
  - Naranja: 0
  - Azul: 5 (luz tenue)
  - Blanco 2: 0
- **Uso**: Período nocturno de descanso

#### 🌆 Modo Anochecer
- **Descripción**: Transición gradual de luz diurna a nocturna
- **Duración**: Configurable (por defecto 15 minutos)
- **Efecto**: Rampa inversa desde valores de día hasta modo noche
- **Uso**: Simulación realista del atardecer natural
- **Automatización**: Se activa automáticamente al atardecer si está configurado

#### ⚡ Modo Tormenta
- **Descripción**: Simulación de relámpagos y ambiente tormentoso
- **Características**:
  - Niveles base: Blanco mínimo (0), Azul tenue (5)
  - Flashes aleatorios (blanco o azul)
  - Intensidad: 160-255
  - Duración de flashes: 40-220 ms
  - Ráfagas de 1-3 flashes
  - Intervalos entre ráfagas: 2-8 segundos
- **Uso**: Efecto visual dramático

### Control del Sistema

#### Control por Botón Físico
- **Ubicación**: GPIO 13
- **Funcionamiento**: Presionar para ciclar entre modos
- **Secuencia**: Día → Amanecer → Noche → Tormenta → Anochecer → Día

#### Control por Interfaz Serial
Comandos disponibles por Monitor Serial:
```
1 - Activar Modo Día
2 - Activar Modo Noche
3 - Activar Modo Tormenta
4 - Activar Modo Amanecer
5 - Activar Modo Anochecer
h - Mostrar menú de ayuda
i - Mostrar información de red
s - Menú de ajustes
```

#### Comandos de Configuración
```
w<ssid>,<pass>        - Configurar WiFi (ej: wMiRed,clave123)
l<lat>,<lon>,<tz>     - Configurar ubicación (ej: l-34.6,-58.4,-3)
a<segundos>           - Duración amanecer (ej: a900)
d<segundos>           - Duración anochecer (ej: d900)
sync                  - Sincronizar hora NTP
auto1                 - Activar automatización
auto0                 - Desactivar automatización
```

### Automatización con Amanecer/Atardecer Real

El sistema puede sincronizarse con los horarios reales de amanecer y atardecer:

#### Configuración Necesaria
1. **Conexión WiFi**: Para sincronización NTP
2. **Ubicación geográfica**: Latitud, longitud y zona horaria
3. **Activar modo automático**: Comando `auto1`

#### Funcionamiento
- Calcula sunrise/sunset usando algoritmo NOAA
- Sincroniza hora vía NTP (pool.ntp.org, time.nist.gov)
- Activa automáticamente modo Amanecer al sunrise
- Activa automáticamente modo Anochecer al sunset
- Recalcula horarios diariamente
- Fallback horario: 07:00-19:00 si no hay datos

#### Ejemplo de Configuración
```cpp
// Por Monitor Serial:
wMiWiFi,contraseña123           // Conectar WiFi
l-34.6037,-58.3816,-3            // Buenos Aires, Argentina (UTC-3)
auto1                            // Activar automatización
sync                             // Sincronizar inmediatamente
```

### Interfaz Web

El sistema incluye un servidor web para configuración y monitoreo:

#### Acceso
- **Por mDNS**: `http://iluminacion.local`
- **Por IP**: Se muestra en Monitor Serial al conectar WiFi

#### Funcionalidades
- **Página principal**: Estado actual del sistema
- **Configuración**: 
  - MQTT (broker, puerto, topic)
  - Ubicación geográfica (lat, lon, zona horaria)
  - Duraciones de transiciones
- **Estado del sistema**: 
  - Modo actual
  - Estado WiFi y MQTT
  - Hora sincronizada
  - Horarios de sunrise/sunset
  - Estado de automatización

### Integración con Home Assistant

#### Protocolo MQTT
- **Auto-discovery**: Publica configuración automáticamente
- **Topic de estado**: `iluminacion` (configurable)
- **Topic de comando**: `iluminacion/set`
- **Tipo de entidad**: Select (selector de modos)

#### Configuración
```cpp
// Por Monitor Serial:
m<host>,<puerto>,<topic>
// Ejemplo:
m192.168.1.100,1883,paladarium/luz
```

#### Comandos MQTT Aceptados
- Por nombre: "Día", "Amanecer", "Noche", "Tormenta", "Anochecer"
- Por número: "1", "2", "3", "4", "5"

#### Ejemplo de Integración HA
```yaml
# El dispositivo aparece automáticamente en Home Assistant
# Entidad: select.iluminacion
# Permite seleccionar entre los 5 modos de iluminación
```

### Características Técnicas Avanzadas

- **Control PWM optimizado**: Cache de valores para evitar escrituras innecesarias
- **Transiciones no bloqueantes**: No interfieren con otros procesos
- **Watchdog deshabilitado**: Evita resets durante operaciones largas
- **Persistencia de configuración**: Preferencias guardadas en NVS
- **Reconexión automática**: WiFi y MQTT con reintento inteligente
- **Debounce de botón**: 50ms para evitar lecturas erróneas

### Beneficios para el Ecosistema

1. **Fotosíntesis optimizada**: Ciclos de luz naturales para plantas
2. **Ritmo circadiano**: Transiciones suaves para animales
3. **Efecto visual**: Simulación realista de condiciones naturales
4. **Eficiencia energética**: Control preciso de intensidad
5. **Automatización completa**: Sin intervención manual necesaria
6. **Monitoreo remoto**: Integración con Home Assistant

## ✨ Características Futuras

- Sistema de alimentación automatizado
- Cámara para monitoreo visual
- Sistema de riego por goteo
- Control de CO2
- Análisis de datos históricos y predicciones

## 🚀 Tecnologías Utilizadas

- ESP32 (AZ-Delivery DevKit v4)
- Home Assistant (con auto-discovery MQTT)
- ESPHome
- Sensores DHT11
- Relés para control de dispositivos
- MQTT para comunicación (PubSubClient)
- Control PWM de LEDs (20 kHz, 8 bits)
- Servidor web integrado (mDNS)
- Sincronización NTP para automatización horaria
- Algoritmo NOAA para cálculo de sunrise/sunset

## 📦 Instalación

(Instrucciones de instalación pendientes)

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor, abre un issue o pull request para sugerencias o mejoras.

## 📄 Licencia

Este proyecto está bajo licencia MIT.

---

**Nota**: Este es un proyecto en desarrollo activo. La documentación y características se actualizan regularmente.