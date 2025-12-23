#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "mqtt_client.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "dht11.h"
#include "wifi_config.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static const char *TAG = "PALADARIO";

// Definición de pines
#define DHT_GPIO 15  // D15 -> GPIO15 en ESP-32D
#define BOMBA_LLUVIA_GPIO 25
#define BOMBA_CASCADA_GPIO 26
#define VENTILADOR_GPIO 27
#define CALEFACCION_GPIO 33

// Variables globales
float temperatura = 0.0f;
float humedad = 0.0f;
bool bomba_lluvia_activa = false;
bool bomba_cascada_activa = false;
bool ventilador_activo = false;
bool calefaccion_activa = false;
bool wifi_conectado = false;
bool dht_valido = false; // Publicar sensores solo tras primera lectura válida

httpd_handle_t server = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;

// Declaraciones
void mqtt_publish_state();
void mqtt_send_discovery();

// GPIO
void config_gpio(void) {
    gpio_set_direction(BOMBA_LLUVIA_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BOMBA_CASCADA_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(VENTILADOR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CALEFACCION_GPIO, GPIO_MODE_OUTPUT);
    // Iniciar apagado (LOW=apagado, HIGH=encendido)
    gpio_set_level(BOMBA_LLUVIA_GPIO, 0);
    gpio_set_level(BOMBA_CASCADA_GPIO, 0);
    gpio_set_level(VENTILADOR_GPIO, 0);
    gpio_set_level(CALEFACCION_GPIO, 0);
    ESP_LOGI(TAG, "GPIOs configurados (logica normal: HIGH=ON, LOW=OFF)");
}

// Control bombas - Lógica NORMAL: HIGH=ON, LOW=OFF
void control_bomba_lluvia(bool activar) {
    gpio_set_level(BOMBA_LLUVIA_GPIO, activar ? 1 : 0);
    bomba_lluvia_activa = activar;
    ESP_LOGI(TAG, "Bomba lluvia: %s (GPIO=%d)", activar ? "ON" : "OFF", activar ? 1 : 0);
}

void control_bomba_cascada(bool activar) {
    gpio_set_level(BOMBA_CASCADA_GPIO, activar ? 1 : 0);
    bomba_cascada_activa = activar;
    ESP_LOGI(TAG, "Bomba cascada: %s (GPIO=%d)", activar ? "ON" : "OFF", activar ? 1 : 0);
}

void control_ventilador(bool activar) {
    gpio_set_level(VENTILADOR_GPIO, activar ? 1 : 0);
    ventilador_activo = activar;
    ESP_LOGI(TAG, "Ventilador: %s (GPIO=%d)", activar ? "ON" : "OFF", activar ? 1 : 0);
}

void control_calefaccion(bool activar) {
    gpio_set_level(CALEFACCION_GPIO, activar ? 1 : 0);
    calefaccion_activa = activar;
    ESP_LOGI(TAG, "Calefaccion: %s (GPIO=%d)", activar ? "ON" : "OFF", activar ? 1 : 0);
}

// Publicar estado MQTT
void mqtt_publish_state() {
    if (mqtt_client == NULL) return;

    // Sensores (solo si hay lectura válida; sin valores por defecto)
    if (dht_valido) {
        char payload[32];
        snprintf(payload, sizeof(payload), "%.1f", temperatura);
        esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/sensor/temperatura/state", payload, 0, 1, 1);
        snprintf(payload, sizeof(payload), "%.1f", humedad);
        esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/sensor/humedad/state", payload, 0, 1, 1);
    }

    esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/switch/bomba_lluvia/state", 
                           bomba_lluvia_activa ? "ON" : "OFF", 0, 1, 1);
    
    esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/switch/bomba_cascada/state", 
                           bomba_cascada_activa ? "ON" : "OFF", 0, 1, 1);
    
    esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/switch/ventilador/state", 
                           ventilador_activo ? "ON" : "OFF", 0, 1, 1);
    
    esp_mqtt_client_publish(mqtt_client, MQTT_BASE_TOPIC"/switch/calefaccion/state", 
                           calefaccion_activa ? "ON" : "OFF", 0, 1, 1);
}

// WiFi handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_conectado = false;
        ESP_LOGI(TAG, "Desconectado, reintentando...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Accede via: http://" IPSTR "/ o http://ecosistema.local/", IP2STR(&event->ip_info.ip));
        wifi_conectado = true;
    }
}

// WiFi init
void wifi_init() {
    // Inicializar NVS con manejo de errores
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Borrando NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Crear interfaz WiFi STA
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    
    // Configurar IP estática
    esp_netif_dhcpc_stop(sta_netif);
    
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 88);           // IP fija: 192.168.1.88
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);            // Gateway (router)
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);     // Máscara de subred
    
    esp_netif_set_ip_info(sta_netif, &ip_info);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi iniciado con IP fija: 192.168.1.88");
}

// MQTT handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_BASE_TOPIC"/switch/bomba_lluvia/set", 0);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_BASE_TOPIC"/switch/bomba_cascada/set", 0);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_BASE_TOPIC"/switch/ventilador/set", 0);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_BASE_TOPIC"/switch/calefaccion/set", 0);
            mqtt_send_discovery();
            mqtt_publish_state();
            break;
            
        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, MQTT_BASE_TOPIC"/switch/bomba_lluvia/set", event->topic_len) == 0) {
                control_bomba_lluvia(strncmp(event->data, "ON", event->data_len) == 0);
                mqtt_publish_state();
            }
            if (strncmp(event->topic, MQTT_BASE_TOPIC"/switch/bomba_cascada/set", event->topic_len) == 0) {
                control_bomba_cascada(strncmp(event->data, "ON", event->data_len) == 0);
                mqtt_publish_state();
            }
            if (strncmp(event->topic, MQTT_BASE_TOPIC"/switch/ventilador/set", event->topic_len) == 0) {
                control_ventilador(strncmp(event->data, "ON", event->data_len) == 0);
                mqtt_publish_state();
            }
            if (strncmp(event->topic, MQTT_BASE_TOPIC"/switch/calefaccion/set", event->topic_len) == 0) {
                control_calefaccion(strncmp(event->data, "ON", event->data_len) == 0);
                mqtt_publish_state();
            }
            break;
            
        default:
            break;
    }
}

// MQTT Discovery
void mqtt_send_discovery() {
    if (mqtt_client == NULL) return;
    
    char payload[512];
    char topic[128];

    // Temperatura
    snprintf(topic, sizeof(topic), "%s/sensor/paladario_temp/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Paladario Temperatura\"," 
             "\"stat_t\":\"%s/sensor/temperatura/state\"," 
             "\"unit_of_meas\":\"°C\"," 
             "\"dev_cla\":\"temperature\"," 
             "\"uniq_id\":\"paladario_temp\"," 
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    // Humedad
    snprintf(topic, sizeof(topic), "%s/sensor/paladario_hum/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Paladario Humedad\"," 
             "\"stat_t\":\"%s/sensor/humedad/state\"," 
             "\"unit_of_meas\":\"%%\"," 
             "\"dev_cla\":\"humidity\"," 
             "\"uniq_id\":\"paladario_hum\"," 
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    // Bomba Lluvia
    snprintf(topic, sizeof(topic), "%s/switch/paladario_lluvia/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Bomba Lluvia\","
             "\"cmd_t\":\"%s/switch/bomba_lluvia/set\","
             "\"stat_t\":\"%s/switch/bomba_lluvia/state\","
             "\"uniq_id\":\"paladario_lluvia\","
             "\"icon\":\"mdi:water\","
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC, MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    // Bomba Cascada
    snprintf(topic, sizeof(topic), "%s/switch/paladario_cascada/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Bomba Cascada\","
             "\"cmd_t\":\"%s/switch/bomba_cascada/set\","
             "\"stat_t\":\"%s/switch/bomba_cascada/state\","
             "\"uniq_id\":\"paladario_cascada\","
             "\"icon\":\"mdi:waterfall\","
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC, MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    // Ventilador
    snprintf(topic, sizeof(topic), "%s/switch/paladario_ventilador/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Ventilador\","
             "\"cmd_t\":\"%s/switch/ventilador/set\","
             "\"stat_t\":\"%s/switch/ventilador/state\","
             "\"uniq_id\":\"paladario_ventilador\","
             "\"icon\":\"mdi:fan\","
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC, MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    // Calefacción
    snprintf(topic, sizeof(topic), "%s/switch/paladario_calefaccion/config", MQTT_DISCOVERY_PREFIX);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Calefaccion\","
             "\"cmd_t\":\"%s/switch/calefaccion/set\","
             "\"stat_t\":\"%s/switch/calefaccion/state\","
             "\"uniq_id\":\"paladario_calefaccion\","
             "\"icon\":\"mdi:radiator\","
             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}",
             MQTT_BASE_TOPIC, MQTT_BASE_TOPIC);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    
    ESP_LOGI(TAG, "Discovery MQTT enviado");
}

// Servidor web nuevo
static esp_err_t root_handler(httpd_req_t *req) {
    const char *html_part1 = 
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Paladario Control</title>"
        "<style>"
        "body{font-family:Arial;margin:0;padding:20px;background:#f0f0f0}"
        ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
        "h1{color:#333;text-align:center;margin-bottom:30px}"
        ".card{background:#f9f9f9;padding:15px;margin:15px 0;border-radius:8px;border-left:4px solid #4CAF50}"
        ".status{font-size:24px;font-weight:bold;margin:10px 0}"
        ".on{color:#4CAF50}"
        ".off{color:#e74c3c}"
        "button{width:100%;padding:15px;font-size:18px;border:none;border-radius:5px;cursor:pointer;margin:5px 0;transition:0.3s}"
        ".btn-on{background:#4CAF50;color:white}"
        ".btn-on:hover{background:#45a049}"
        ".btn-off{background:#e74c3c;color:white}"
        ".btn-off:hover{background:#c0392b}"
        ".info{text-align:center;color:#666;margin-top:20px;font-size:14px}"
        "</style>"
        "</head><body>"
        "<div class='container'>"
        "<h1>🌿 Paladario Control</h1>";
    
    httpd_resp_send_chunk(req, html_part1, strlen(html_part1));
    
    char html_lluvia[400];
    snprintf(html_lluvia, sizeof(html_lluvia),
        "<div class='card'>"
        "<h2>💧 Bomba Lluvia</h2>"
        "<div class='status %s'>%s</div>"
        "<form action='/lluvia' method='post'>"
        "<button type='submit' name='action' value='on' class='btn-on'>ENCENDER</button>"
        "<button type='submit' name='action' value='off' class='btn-off'>APAGAR</button>"
        "</form>"
        "</div>",
        bomba_lluvia_activa ? "on" : "off",
        bomba_lluvia_activa ? "● ENCENDIDA" : "○ APAGADA");
    
    httpd_resp_send_chunk(req, html_lluvia, strlen(html_lluvia));
    
    char html_cascada[400];
    snprintf(html_cascada, sizeof(html_cascada),
        "<div class='card'>"
        "<h2>🌊 Bomba Cascada</h2>"
        "<div class='status %s'>%s</div>"
        "<form action='/cascada' method='post'>"
        "<button type='submit' name='action' value='on' class='btn-on'>ENCENDER</button>"
        "<button type='submit' name='action' value='off' class='btn-off'>APAGAR</button>"
        "</form>"
        "</div>",
        bomba_cascada_activa ? "on" : "off",
        bomba_cascada_activa ? "● ENCENDIDA" : "○ APAGADA");
    
    httpd_resp_send_chunk(req, html_cascada, strlen(html_cascada));
    
    char html_ventilador[400];
    snprintf(html_ventilador, sizeof(html_ventilador),
        "<div class='card'>"
        "<h2>🌬️ Ventilador</h2>"
        "<div class='status %s'>%s</div>"
        "<form action='/ventilador' method='post'>"
        "<button type='submit' name='action' value='on' class='btn-on'>ENCENDER</button>"
        "<button type='submit' name='action' value='off' class='btn-off'>APAGAR</button>"
        "</form>"
        "</div>",
        ventilador_activo ? "on" : "off",
        ventilador_activo ? "● ENCENDIDO" : "○ APAGADO");
    
    httpd_resp_send_chunk(req, html_ventilador, strlen(html_ventilador));
    
    char html_calefaccion[400];
    snprintf(html_calefaccion, sizeof(html_calefaccion),
        "<div class='card'>"
        "<h2>🔥 Calefacción</h2>"
        "<div class='status %s'>%s</div>"
        "<form action='/calefaccion' method='post'>"
        "<button type='submit' name='action' value='on' class='btn-on'>ENCENDER</button>"
        "<button type='submit' name='action' value='off' class='btn-off'>APAGAR</button>"
        "</form>"
        "</div>",
        calefaccion_activa ? "on" : "off",
        calefaccion_activa ? "● ENCENDIDA" : "○ APAGADA");
    
    httpd_resp_send_chunk(req, html_calefaccion, strlen(html_calefaccion));
    
    const char *html_sensor = 
        "<div class='card'>"
        "<h2>📊 Sensores</h2>"
        "<p>🌡️ Temperatura: <strong>%.1f°C</strong></p>"
        "<p>💦 Humedad: <strong>%.1f%%</strong></p>"
        "</div>";

    char sensor_buf[200];
    int n = snprintf(sensor_buf, sizeof(sensor_buf), html_sensor, temperatura, humedad);
    httpd_resp_send_chunk(req, sensor_buf, n);

    const char *html_ota = 
        "<div class='card'>"
        "<h2>🔄 Actualización OTA</h2>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='firmware' accept='.bin' style='width:100%;padding:10px;margin:10px 0'>"
        "<button type='submit' style='background:#2196F3;color:white;width:100%;padding:15px;font-size:18px;border:none;border-radius:5px;cursor:pointer'>📤 Subir Firmware</button>"
        "</form>"
        "</div>";
    httpd_resp_send_chunk(req, html_ota, strlen(html_ota));
    
    const char *html_end = 
        "<div class='info'>"
        "<p>Sistema ESP32 Paladario v1.0</p>"
        "<p><a href='/'>↻ Actualizar</a></p>"
        "</div>"
        "</div>"
        "</body></html>";
    
    httpd_resp_send_chunk(req, html_end, strlen(html_end));
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t lluvia_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content));
    
    if (ret > 0) {
        if (strstr(content, "action=on")) {
            control_bomba_lluvia(true);
        } else if (strstr(content, "action=off")) {
            control_bomba_lluvia(false);
        }
        mqtt_publish_state();
    }
    
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t cascada_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content));
    
    if (ret > 0) {
        if (strstr(content, "action=on")) {
            control_bomba_cascada(true);
        } else if (strstr(content, "action=off")) {
            control_bomba_cascada(false);
        }
        mqtt_publish_state();
    }
    
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t ventilador_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content));
    
    if (ret > 0) {
        if (strstr(content, "action=on")) {
            control_ventilador(true);
        } else if (strstr(content, "action=off")) {
            control_ventilador(false);
        }
        mqtt_publish_state();
    }
    
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t calefaccion_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content));
    
    if (ret > 0) {
        if (strstr(content, "action=on")) {
            control_calefaccion(true);
        } else if (strstr(content, "action=off")) {
            control_calefaccion(false);
        }
        mqtt_publish_state();
    }
    
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// OTA Update handler
static esp_err_t ota_handler(httpd_req_t *req) {
    char buf[1024];
    int received;
    int remaining = req->content_len;
    
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    
    if (update_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Iniciando OTA...");
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    
    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }
        
        if (esp_ota_write(ota_handle, buf, received) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        remaining -= received;
    }
    
    if (esp_ota_end(ota_handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }
    
    httpd_resp_sendstr(req, "Update OK! Reiniciando...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 16;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };
        
        httpd_uri_t lluvia = {
            .uri = "/lluvia",
            .method = HTTP_POST,
            .handler = lluvia_handler
        };
        
        httpd_uri_t cascada = {
            .uri = "/cascada",
            .method = HTTP_POST,
            .handler = cascada_handler
        };
        
        httpd_uri_t ventilador = {
            .uri = "/ventilador",
            .method = HTTP_POST,
            .handler = ventilador_handler
        };
        
        httpd_uri_t calefaccion = {
            .uri = "/calefaccion",
            .method = HTTP_POST,
            .handler = calefaccion_handler
        };
        
        httpd_uri_t ota = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = ota_handler
        };
        
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &lluvia);
        httpd_register_uri_handler(server, &cascada);
        httpd_register_uri_handler(server, &ventilador);
        httpd_register_uri_handler(server, &calefaccion);
        httpd_register_uri_handler(server, &ota);
        
        ESP_LOGI(TAG, "Servidor web iniciado con OTA");
    }
}

// MQTT init
void mqtt_init() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" MQTT_BROKER,
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS,
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT iniciado");
}

// Tarea sensor (DHT22 AM2302 en GPIO4)
void task_sensor(void *pvParameter) {
    // Configurar GPIO con pull-up interno
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Sensor DHT22 (AM2302) en GPIO %d", DHT_GPIO);
    vTaskDelay(pdMS_TO_TICKS(3000)); // estabilización un poco mayor

    int estado_inicial = gpio_get_level(DHT_GPIO);
    ESP_LOGI(TAG, "GPIO%d estado inicial: %s (esperado: HIGH por pull-up)",
             DHT_GPIO, estado_inicial ? "HIGH" : "LOW");
    if (estado_inicial == 0) {
        ESP_LOGW(TAG, "DATA en LOW. Revisa: DATA a GPIO%d, VCC=3V3, GND común, y que el módulo tenga pull-up.", DHT_GPIO);
    }

    int errores = 0;
    while (1) {
        float h = 0, t = 0;
        esp_err_t res = ESP_FAIL;
        for (int intento = 0; intento < 3; intento++) {
            res = dht11_read(DHT_GPIO, &h, &t);
            if (res == ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (res == ESP_OK) {
            temperatura = t;
            humedad = h;
            dht_valido = true;
            errores = 0;
            ESP_LOGI(TAG, "DHT22 OK: T=%.1f°C H=%.1f%%", temperatura, humedad);
            if (wifi_conectado && mqtt_client) {
                mqtt_publish_state();
            }
        } else {
            errores++;
            ESP_LOGW(TAG, "DHT22 fallo (%d)", errores);
            // No modificar temperatura/humedad: sin valores por defecto
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// Tarea estado
void task_estado(void *pvParameter) {
    while (1) {
        ESP_LOGI(TAG, "T:%.1fC H:%.1f%% Lluvia:%s Cascada:%s Vent:%s Calef:%s",
                temperatura, humedad,
                bomba_lluvia_activa ? "ON" : "OFF",
                bomba_cascada_activa ? "ON" : "OFF",
                ventilador_activo ? "ON" : "OFF",
                calefaccion_activa ? "ON" : "OFF");
        vTaskDelay(pdMS_TO_TICKS(15000)); // Cada 15s
    }
}

void app_main() {
    ESP_LOGI(TAG, "=== PALADARIO MQTT ===");
    
    config_gpio();
    wifi_init();
    
    ESP_LOGI(TAG, "Esperando WiFi...");
    int timeout = 20;
    while (!wifi_conectado && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
    }
    
    if (wifi_conectado) {
        start_webserver();
        mqtt_init();
        ESP_LOGI(TAG, "Sistema listo - Control por Home Assistant");
    } else {
        ESP_LOGW(TAG, "Sin WiFi - Modo offline");
    }
    
    xTaskCreate(&task_sensor, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(&task_estado, "estado", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Sistema iniciado");
}
