#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_log.h"

#define DHT11_TIMEOUT 8000

static const char *TAG = "DHT22";

static int wait_for_state(gpio_num_t pin, int state, int timeout) {
    int count = 0;
    while (gpio_get_level(pin) != state) {
        if (count++ > timeout) {
            return -1;
        }
        ets_delay_us(1);
    }
    return count;
}

esp_err_t dht11_read(gpio_num_t pin, float *humidity, float *temperature) {
    uint8_t data[5] = {0};
    
    // Señal de inicio: configurar como salida
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));  // Estabilización previa al start
    
    // Deshabilitar interrupciones
    taskDISABLE_INTERRUPTS();
    
    // Señal de inicio: LOW por ~20ms (especificación DHT22)
    gpio_set_level(pin, 0);
    ets_delay_us(20000);
    
    // HIGH por ~40us
    gpio_set_level(pin, 1);
    ets_delay_us(40);
    
    // Cambiar a entrada y asegurar pull-up interno
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    
    // Esperar respuesta del sensor: LOW de 80us
    if (wait_for_state(pin, 0, DHT11_TIMEOUT) < 0) {
        taskENABLE_INTERRUPTS();
        ESP_LOGE(TAG, "Timeout: Sensor no responde (esperando LOW)");
        return ESP_FAIL;
    }
    
    // Esperar HIGH de 80us
    if (wait_for_state(pin, 1, DHT11_TIMEOUT) < 0) {
        taskENABLE_INTERRUPTS();
        ESP_LOGE(TAG, "Timeout: Sensor no envía HIGH de confirmación");
        return ESP_FAIL;
    }
    
    // Esperar que vuelva a LOW para comenzar transmisión
    if (wait_for_state(pin, 0, DHT11_TIMEOUT) < 0) {
        taskENABLE_INTERRUPTS();
        ESP_LOGE(TAG, "Timeout: Inicio de transmisión");
        return ESP_FAIL;
    }
    
    // Leer 40 bits de datos
    for (int i = 0; i < 40; i++) {
        // Esperar HIGH (inicio de bit)
        if (wait_for_state(pin, 1, DHT11_TIMEOUT) < 0) {
            taskENABLE_INTERRUPTS();
            ESP_LOGE(TAG, "Timeout en bit %d (esperando HIGH)", i);
            return ESP_FAIL;
        }
        
        // Medir duración del pulso HIGH
        int high_time = wait_for_state(pin, 0, DHT11_TIMEOUT);
        if (high_time < 0) {
            taskENABLE_INTERRUPTS();
            ESP_LOGE(TAG, "Timeout en bit %d (esperando LOW)", i);
            return ESP_FAIL;
        }
        
        // Determinar si es 0 o 1 (>50us = 1, <50us = 0)
        data[i / 8] <<= 1;
        if (high_time > 50) {
            data[i / 8] |= 1;
        }
    }
    
    taskENABLE_INTERRUPTS();
    
    // Verificar checksum
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (data[4] != checksum) {
        ESP_LOGE(TAG, "Checksum error: calc=0x%02X recv=0x%02X", checksum, data[4]);
        ESP_LOGE(TAG, "Raw: %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3], data[4]);
        return ESP_FAIL;
    }
    // Lectura nula (todos ceros) no es válida
    if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0 && data[4] == 0) {
        ESP_LOGE(TAG, "Lectura nula: Raw 00 00 00 00 00");
        return ESP_FAIL;
    }
    
    // Extraer valores (DHT22: 16 bits por valor, con decimales)
    int16_t hum = (data[0] << 8) | data[1];
    int16_t temp = (data[2] << 8) | data[3];
    
    // DHT22 puede tener temperatura negativa (bit más alto indica signo)
    if (temp & 0x8000) {
        temp = -(temp & 0x7FFF);
    }
    
    *humidity = (float)hum / 10.0f;
    *temperature = (float)temp / 10.0f;
    
    // Validar rangos DHT22: Humedad 0-100%, Temperatura -40 a 80°C
    if (*humidity < 0.0f || *humidity > 100.0f || *temperature < -40.0f || *temperature > 80.0f) {
        ESP_LOGW(TAG, "Valores fuera de rango: H=%.1f%% T=%.1f°C", *humidity, *temperature);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ DHT22: H=%.1f%% T=%.1f°C (Raw %02X %02X %02X %02X %02X)", *humidity, *temperature,
             data[0], data[1], data[2], data[3], data[4]);
    
    return ESP_OK;
}
// DHT22 (AM2302) reader implemented here
