#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ========================================
// CONFIGURACIÓN WIFI - EDITAR AQUÍ
// ========================================

#define WIFI_SSID "MiFibra"        // Cambiar por el nombre de tu red WiFi
#define WIFI_PASSWORD "rZ75dCAn"  // Cambiar por tu contraseña WiFi

// ========================================
// CONFIGURACIÓN MQTT (Home Assistant)
// ========================================

#define MQTT_BROKER "192.168.1.132"    // IP de tu Home Assistant
#define MQTT_PORT 1883
#define MQTT_USER "iluminacion"      // Usuario MQTT (opcional)
#define MQTT_PASS "Thosiba2010"   // Contraseña MQTT (opcional)

// Tópicos MQTT
#define MQTT_BASE_TOPIC "paladario"
#define MQTT_DISCOVERY_PREFIX "homeassistant"

#endif // WIFI_CONFIG_H
