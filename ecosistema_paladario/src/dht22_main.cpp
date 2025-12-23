#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// Configuración WiFi
const char* ssid = "MiFibra";
const char* password = "rZ75dCAn";
IPAddress local_IP(192, 168, 1, 88);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Configuración MQTT
const char* mqtt_server = "192.168.1.132";
const int mqtt_port = 1883;
const char* mqtt_user = "iluminacion";
const char* mqtt_password = "Thosiba2010";
const char* mqtt_base_topic = "paladario";

// Configuración del sensor DHT22
#define DHTPIN 15     // Pin D15 (GPIO 15)
#define DHTTYPE DHT22 // Tipo de sensor DHT22

// Definición de pines de relés
#define BOMBA_LLUVIA_PIN 25
#define BOMBA_CASCADA_PIN 26
#define VENTILADOR_PIN 27
#define CALEFACCION_PIN 33

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Variables globales
float temperatura = 0.0;
float humedad = 0.0;
bool bomba_lluvia = false;
bool bomba_cascada = false;
bool ventilador = false;
bool calefaccion = false;
bool mqtt_conectado = false;

// Declaraciones de funciones
void enviarMqttDiscovery();
void publicarEstados();

// Funciones de control de relés
void controlarBombaLluvia(bool estado) {
  digitalWrite(BOMBA_LLUVIA_PIN, estado ? HIGH : LOW);
  bomba_lluvia = estado;
  Serial.println("Bomba Lluvia: " + String(estado ? "ON" : "OFF"));
  
  // Publicar estado a MQTT
  if (mqtt_conectado) {
    mqtt.publish((String(mqtt_base_topic) + "/switch/bomba_lluvia/state").c_str(), 
                 estado ? "ON" : "OFF", true);
  }
}

void controlarBombaCascada(bool estado) {
  digitalWrite(BOMBA_CASCADA_PIN, estado ? HIGH : LOW);
  bomba_cascada = estado;
  Serial.println("Bomba Cascada: " + String(estado ? "ON" : "OFF"));
  
  if (mqtt_conectado) {
    mqtt.publish((String(mqtt_base_topic) + "/switch/bomba_cascada/state").c_str(), 
                 estado ? "ON" : "OFF", true);
  }
}

void controlarVentilador(bool estado) {
  digitalWrite(VENTILADOR_PIN, estado ? HIGH : LOW);
  ventilador = estado;
  Serial.println("Ventilador: " + String(estado ? "ON" : "OFF"));
  
  if (mqtt_conectado) {
    mqtt.publish((String(mqtt_base_topic) + "/switch/ventilador/state").c_str(), 
                 estado ? "ON" : "OFF", true);
  }
}

void controlarCalefaccion(bool estado) {
  digitalWrite(CALEFACCION_PIN, estado ? HIGH : LOW);
  calefaccion = estado;
  Serial.println("Calefacción: " + String(estado ? "ON" : "OFF"));
  
  if (mqtt_conectado) {
    mqtt.publish((String(mqtt_base_topic) + "/switch/calefaccion/state").c_str(), 
                 estado ? "ON" : "OFF", true);
  }
}

// Callback MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("MQTT recibido [" + String(topic) + "]: " + message);
  
  String topicStr = String(topic);
  bool estado = (message == "ON");
  
  if (topicStr.endsWith("/bomba_lluvia/set")) {
    controlarBombaLluvia(estado);
  } else if (topicStr.endsWith("/bomba_cascada/set")) {
    controlarBombaCascada(estado);
  } else if (topicStr.endsWith("/ventilador/set")) {
    controlarVentilador(estado);
  } else if (topicStr.endsWith("/calefaccion/set")) {
    controlarCalefaccion(estado);
  }
}

// Conectar a MQTT y enviar discovery
void mqttConnect() {
  while (!mqtt.connected()) {
    Serial.print("Conectando a MQTT...");
    
    if (mqtt.connect("ESP32_Paladario", mqtt_user, mqtt_password)) {
      Serial.println(" ✓ Conectado");
      mqtt_conectado = true;
      
      // Suscribirse a topics de control
      mqtt.subscribe((String(mqtt_base_topic) + "/switch/bomba_lluvia/set").c_str());
      mqtt.subscribe((String(mqtt_base_topic) + "/switch/bomba_cascada/set").c_str());
      mqtt.subscribe((String(mqtt_base_topic) + "/switch/ventilador/set").c_str());
      mqtt.subscribe((String(mqtt_base_topic) + "/switch/calefaccion/set").c_str());
      
      // Enviar MQTT Discovery para Home Assistant
      enviarMqttDiscovery();
      
      // Publicar estados iniciales
      publicarEstados();
      
    } else {
      Serial.println(" ✗ Error, reintentando en 5s");
      mqtt_conectado = false;
      delay(5000);
    }
  }
}

// Enviar configuración MQTT Discovery
void enviarMqttDiscovery() {
  Serial.println("Enviando MQTT Discovery...");
  
  // Sensor de Temperatura
  String configTemp = "{\"name\":\"Paladario Temperatura\","
                      "\"stat_t\":\"" + String(mqtt_base_topic) + "/sensor/temperatura/state\","
                      "\"unit_of_meas\":\"°C\","
                      "\"dev_cla\":\"temperature\","
                      "\"val_tpl\":\"{{ value }}\","
                      "\"uniq_id\":\"paladario_temp\","
                      "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/sensor/paladario_temp/config", configTemp.c_str(), true);
  
  // Sensor de Humedad
  String configHum = "{\"name\":\"Paladario Humedad\","
                     "\"stat_t\":\"" + String(mqtt_base_topic) + "/sensor/humedad/state\","
                     "\"unit_of_meas\":\"%\","
                     "\"dev_cla\":\"humidity\","
                     "\"val_tpl\":\"{{ value }}\","
                     "\"uniq_id\":\"paladario_hum\","
                     "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/sensor/paladario_hum/config", configHum.c_str(), true);
  
  // Switch Bomba Lluvia
  String configLluvia = "{\"name\":\"Bomba Lluvia\","
                        "\"cmd_t\":\"" + String(mqtt_base_topic) + "/switch/bomba_lluvia/set\","
                        "\"stat_t\":\"" + String(mqtt_base_topic) + "/switch/bomba_lluvia/state\","
                        "\"icon\":\"mdi:water\","
                        "\"uniq_id\":\"paladario_lluvia\","
                        "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/switch/paladario_lluvia/config", configLluvia.c_str(), true);
  
  // Switch Bomba Cascada
  String configCascada = "{\"name\":\"Bomba Cascada\","
                         "\"cmd_t\":\"" + String(mqtt_base_topic) + "/switch/bomba_cascada/set\","
                         "\"stat_t\":\"" + String(mqtt_base_topic) + "/switch/bomba_cascada/state\","
                         "\"icon\":\"mdi:waterfall\","
                         "\"uniq_id\":\"paladario_cascada\","
                         "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/switch/paladario_cascada/config", configCascada.c_str(), true);
  
  // Switch Ventilador
  String configVentilador = "{\"name\":\"Ventilador\","
                            "\"cmd_t\":\"" + String(mqtt_base_topic) + "/switch/ventilador/set\","
                            "\"stat_t\":\"" + String(mqtt_base_topic) + "/switch/ventilador/state\","
                            "\"icon\":\"mdi:fan\","
                            "\"uniq_id\":\"paladario_ventilador\","
                            "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/switch/paladario_ventilador/config", configVentilador.c_str(), true);
  
  // Switch Calefacción
  String configCalefaccion = "{\"name\":\"Calefacción\","
                             "\"cmd_t\":\"" + String(mqtt_base_topic) + "/switch/calefaccion/set\","
                             "\"stat_t\":\"" + String(mqtt_base_topic) + "/switch/calefaccion/state\","
                             "\"icon\":\"mdi:radiator\","
                             "\"uniq_id\":\"paladario_calefaccion\","
                             "\"dev\":{\"ids\":[\"paladario\"],\"name\":\"Paladario\",\"mf\":\"DIY\",\"mdl\":\"ESP32\"}}";
  mqtt.publish("homeassistant/switch/paladario_calefaccion/config", configCalefaccion.c_str(), true);
  
  Serial.println("✓ MQTT Discovery enviado");
}

// Publicar estados actuales
void publicarEstados() {
  if (!mqtt_conectado) return;
  
  // Publicar temperatura y humedad
  mqtt.publish((String(mqtt_base_topic) + "/sensor/temperatura/state").c_str(), 
               String(temperatura, 1).c_str(), true);
  mqtt.publish((String(mqtt_base_topic) + "/sensor/humedad/state").c_str(), 
               String(humedad, 1).c_str(), true);
  
  // Publicar estados de switches
  mqtt.publish((String(mqtt_base_topic) + "/switch/bomba_lluvia/state").c_str(), 
               bomba_lluvia ? "ON" : "OFF", true);
  mqtt.publish((String(mqtt_base_topic) + "/switch/bomba_cascada/state").c_str(), 
               bomba_cascada ? "ON" : "OFF", true);
  mqtt.publish((String(mqtt_base_topic) + "/switch/ventilador/state").c_str(), 
               ventilador ? "ON" : "OFF", true);
  mqtt.publish((String(mqtt_base_topic) + "/switch/calefaccion/state").c_str(), 
               calefaccion ? "ON" : "OFF", true);
}

// Página web principal
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Paladario Control</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:0;padding:20px;background:#f0f0f0}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;text-align:center;margin-bottom:30px}";
  html += ".card{background:#f9f9f9;padding:15px;margin:15px 0;border-radius:8px;border-left:4px solid #4CAF50}";
  html += ".status{font-size:20px;font-weight:bold;margin:10px 0}";
  html += ".on{color:#4CAF50}";
  html += ".off{color:#e74c3c}";
  html += "button{width:48%;padding:15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:2px;transition:0.3s}";
  html += ".btn-on{background:#4CAF50;color:white}";
  html += ".btn-on:hover{background:#45a049}";
  html += ".btn-off{background:#e74c3c;color:white}";
  html += ".btn-off:hover{background:#c0392b}";
  html += ".sensor{font-size:18px;margin:10px 0}";
  html += "</style>";
  html += "<script>function control(device,action){fetch('/'+device+'?action='+action).then(()=>location.reload());}</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🌿 Paladario Control</h1>";
  
  // Sensores
  html += "<div class='card'>";
  html += "<h2>📊 Sensores DHT22</h2>";
  html += "<div class='sensor'>🌡️ Temperatura: <strong>" + String(temperatura, 1) + " °C</strong></div>";
  html += "<div class='sensor'>💦 Humedad: <strong>" + String(humedad, 1) + " %</strong></div>";
  html += "</div>";
  
  // Bomba Lluvia
  html += "<div class='card'>";
  html += "<h2>💧 Bomba Lluvia</h2>";
  html += "<div class='status " + String(bomba_lluvia ? "on" : "off") + "'>";
  html += bomba_lluvia ? "● ENCENDIDA" : "○ APAGADA";
  html += "</div>";
  html += "<button class='btn-on' onclick='control(\"lluvia\",\"on\")'>ENCENDER</button>";
  html += "<button class='btn-off' onclick='control(\"lluvia\",\"off\")'>APAGAR</button>";
  html += "</div>";
  
  // Bomba Cascada
  html += "<div class='card'>";
  html += "<h2>🌊 Bomba Cascada</h2>";
  html += "<div class='status " + String(bomba_cascada ? "on" : "off") + "'>";
  html += bomba_cascada ? "● ENCENDIDA" : "○ APAGADA";
  html += "</div>";
  html += "<button class='btn-on' onclick='control(\"cascada\",\"on\")'>ENCENDER</button>";
  html += "<button class='btn-off' onclick='control(\"cascada\",\"off\")'>APAGAR</button>";
  html += "</div>";
  
  // Ventilador
  html += "<div class='card'>";
  html += "<h2>🌬️ Ventilador</h2>";
  html += "<div class='status " + String(ventilador ? "on" : "off") + "'>";
  html += ventilador ? "● ENCENDIDO" : "○ APAGADO";
  html += "</div>";
  html += "<button class='btn-on' onclick='control(\"ventilador\",\"on\")'>ENCENDER</button>";
  html += "<button class='btn-off' onclick='control(\"ventilador\",\"off\")'>APAGAR</button>";
  html += "</div>";
  
  // Calefacción
  html += "<div class='card'>";
  html += "<h2>🔥 Calefacción</h2>";
  html += "<div class='status " + String(calefaccion ? "on" : "off") + "'>";
  html += calefaccion ? "● ENCENDIDA" : "○ APAGADA";
  html += "</div>";
  html += "<button class='btn-on' onclick='control(\"calefaccion\",\"on\")'>ENCENDER</button>";
  html += "<button class='btn-off' onclick='control(\"calefaccion\",\"off\")'>APAGAR</button>";
  html += "</div>";
  
  html += "<div style='text-align:center;margin-top:20px;color:#666'>";
  html += "<p>IP: 192.168.1.88 | WiFi: " + String(ssid) + "</p>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Manejadores de control
void handleLluvia() {
  if (server.hasArg("action")) {
    controlarBombaLluvia(server.arg("action") == "on");
  }
  server.send(200, "text/plain", "OK");
}

void handleCascada() {
  if (server.hasArg("action")) {
    controlarBombaCascada(server.arg("action") == "on");
  }
  server.send(200, "text/plain", "OK");
}

void handleVentilador() {
  if (server.hasArg("action")) {
    controlarVentilador(server.arg("action") == "on");
  }
  server.send(200, "text/plain", "OK");
}

void handleCalefaccion() {
  if (server.hasArg("action")) {
    controlarCalefaccion(server.arg("action") == "on");
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nIniciando Paladario...");
  
  // Configurar pines de relés
  pinMode(BOMBA_LLUVIA_PIN, OUTPUT);
  pinMode(BOMBA_CASCADA_PIN, OUTPUT);
  pinMode(VENTILADOR_PIN, OUTPUT);
  pinMode(CALEFACCION_PIN, OUTPUT);
  
  // Iniciar todos los relés apagados
  digitalWrite(BOMBA_LLUVIA_PIN, LOW);
  digitalWrite(BOMBA_CASCADA_PIN, LOW);
  digitalWrite(VENTILADOR_PIN, LOW);
  digitalWrite(CALEFACCION_PIN, LOW);
  
  // Iniciar DHT22
  dht.begin();
  Serial.println("Sensor DHT22 iniciado");
  
  // Configurar IP estática
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Error configurando IP estática");
  }
  
  // Conectar a WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("Accede desde: http://192.168.1.88");
  } else {
    Serial.println("\n✗ Error conectando WiFi");
  }
  
  // Configurar MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  
  // Configurar servidor web
  server.on("/", handleRoot);
  server.on("/lluvia", handleLluvia);
  server.on("/cascada", handleCascada);
  server.on("/ventilador", handleVentilador);
  server.on("/calefaccion", handleCalefaccion);
  
  server.begin();
  Serial.println("Servidor web iniciado");
  
  delay(2000); // Esperar a que el sensor se estabilice
}

void loop() {
  // Mantener conexión MQTT
  if (!mqtt.connected()) {
    mqttConnect();
  }
  mqtt.loop();
  
  // Manejar peticiones web
  server.handleClient();
  
  // Leer sensor cada 2 segundos
  static unsigned long ultimaLectura = 0;
  if (millis() - ultimaLectura >= 2000) {
    ultimaLectura = millis();
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
    if (!isnan(h) && !isnan(t)) {
      temperatura = t;
      humedad = h;
      
      Serial.println("================================");
      Serial.print("Temperatura: ");
      Serial.print(temperatura);
      Serial.println(" °C");
      Serial.print("Humedad: ");
      Serial.print(humedad);
      Serial.println(" %");
      Serial.println("================================");
      
      // Publicar sensores a MQTT
      if (mqtt_conectado) {
        mqtt.publish((String(mqtt_base_topic) + "/sensor/temperatura/state").c_str(), 
                     String(temperatura, 1).c_str(), true);
        mqtt.publish((String(mqtt_base_topic) + "/sensor/humedad/state").c_str(), 
                     String(humedad, 1).c_str(), true);
      }
    } else {
      Serial.println("Error al leer el sensor DHT22!");
    }
  }
}
