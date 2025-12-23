#include <Arduino.h>
// Sketch Arduino de prueba DHT22 sin librerías externas (GPIO15)

static const int pinDHT = 15; // D15 -> GPIO15 en ESP32

static int waitForState(int pin, int state, int timeoutUs) {
  int count = 0;
  while (digitalRead(pin) != state) {
    if (count++ > timeoutUs) return -1;
    delayMicroseconds(1);
  }
  return count;
}

bool dht22Read(int pin, float &humidity, float &temperature) {
  uint8_t data[5] = {0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(100);

  noInterrupts();
  // Start: LOW 20ms
  digitalWrite(pin, LOW);
  delayMicroseconds(20000);
  // HIGH 40us
  digitalWrite(pin, HIGH);
  delayMicroseconds(40);
  pinMode(pin, INPUT_PULLUP);

  // Response: 80us LOW, 80us HIGH, then LOW to start bits
  if (waitForState(pin, LOW, 8000) < 0) { interrupts(); return false; }
  if (waitForState(pin, HIGH, 8000) < 0) { interrupts(); return false; }
  if (waitForState(pin, LOW, 8000) < 0) { interrupts(); return false; }

  for (int i = 0; i < 40; i++) {
    if (waitForState(pin, HIGH, 8000) < 0) { interrupts(); return false; }
    int highTime = waitForState(pin, LOW, 8000);
    if (highTime < 0) { interrupts(); return false; }
    data[i / 8] <<= 1;
    if (highTime > 50) data[i / 8] |= 1; // >50us -> 1
  }
  interrupts();

  uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
  if (data[4] != checksum) return false;

  int16_t hum = (data[0] << 8) | data[1];
  int16_t temp = (data[2] << 8) | data[3];
  if (temp & 0x8000) temp = -(temp & 0x7FFF);

  humidity = hum / 10.0f;
  temperature = temp / 10.0f;
  if (humidity < 0 || humidity > 100 || temperature < -40 || temperature > 80) return false;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(pinDHT, INPUT_PULLUP);
  Serial.println("DHT22 (sin librería) en GPIO15");
}

void loop() {
  float t = NAN, h = NAN;
  if (dht22Read(pinDHT, h, t)) {
    Serial.print("Temperatura: ");
    Serial.print(t, 2);
    Serial.println(" °C");
    Serial.print("Humedad: ");
    Serial.print(h, 1);
    Serial.println(" %");
  } else {
    Serial.println("Lectura DHT fallida");
  }
  Serial.println("---");
  delay(1000);
}
