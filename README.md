# 🌿 Paladarium - Sistema de Ecosistema Automatizado

Un proyecto de automatización para paladarium/terrario con control de clima, iluminación y monitoreo integrado con Home Assistant.

## 📋 Descripción

Este proyecto implementa un sistema completo de control y monitoreo para un paladarium, utilizando ESP32 y sensores diversos para mantener condiciones óptimas para plantas y animales. La integración con Home Assistant permite el control remoto y la visualización de datos en tiempo real.

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

## ✨ Características Futuras

- Sistema de alimentación automatizado
- Cámara para monitoreo visual
- Sistema de riego por goteo
- Control de CO2
- Análisis de datos históricos y predicciones

## 🚀 Tecnologías Utilizadas

- ESP32
- Home Assistant
- ESPHome
- Sensores DHT11
- Relés para control de dispositivos
- MQTT para comunicación

## 📦 Instalación

(Instrucciones de instalación pendientes)

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor, abre un issue o pull request para sugerencias o mejoras.

## 📄 Licencia

Este proyecto está bajo licencia MIT.

---

**Nota**: Este es un proyecto en desarrollo activo. La documentación y características se actualizan regularmente.