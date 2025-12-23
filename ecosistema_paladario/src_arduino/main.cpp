#include <Arduino.h>
#include <DHTesp.h>

static const int pinDHT = 15; // D15 -> GPIO15 en ESP32
DHTesp dht;

void setup() {
  Serial.begin(115200);
  delay(200);
  dht.setup(pinDHT, DHTesp::DHT22);
  Serial.println("DHT22 inicializado en GPIO15 (D15)");
}

void loop() {
  TempAndHumidity data = dht.getTempAndHumidity();
  // La librería devuelve NaN si la lectura falla; no inventamos valores
  if (!isnan(data.temperature) && !isnan(data.humidity)) {
    Serial.print("Temperatura: ");
    Serial.print(String(data.temperature, 2));
    Serial.println(" °C");
    Serial.print("Humedad: ");
    Serial.print(String(data.humidity, 1));
    Serial.println(" %");
  } else {
    Serial.println("Lectura DHT fallida");
  }
  Serial.println("---");
  delay(1000);
}
