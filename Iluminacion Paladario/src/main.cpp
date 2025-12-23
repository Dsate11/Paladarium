#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Pines asignados
const int pinBlanco1 = 5;
const int pinNaranja = 18;
const int pinAzul    = 19;
const int pinBlanco2 = 22;

// Canales PWM
const int canalBlanco1 = 0;
const int canalNaranja = 1;
const int canalAzul    = 2;
const int canalBlanco2 = 3;

// Resolución PWM
const int frecuencia = 20000; // Aumentado a 20 kHz para evitar parpadeo
const int resolucion = 8; // 0–255

// Prototipos
void modoNoche();
void modoDia();
void enterStorm();
void handleStorm();
void enterAmanecer();
void handleAmanecer();
void showMenu();
void handleSerialInput();
void showSettingsMenu();
// Prototipos para sincronización y triggers
bool syncTimeAndCalculateSun();
void checkSunTriggersNow();
void applyDayNightBasedOnSun();
// Prototipos para modo anochecer
void enterAnochecer();
void handleAnochecer();
// Prototipos para persistencia y MQTT
void loadSavedWiFi();
void loadMQTTConfig();
void saveMQTTConfig();
bool connectMQTT();
void handleMQTT();
void publishState();
void mqttCallback(char* topic, byte* payload, unsigned int length);
// Prototipos para servidor web
void setupWebServer();
void handleRoot();
void handleConfig();
void handleConfigSave();
void handleStatus();
void handleNotFound();

// Nombre mDNS del dispositivo
const char* mdnsName = "iluminacion";

// Botón para alternar modos (usa pull-up)
const int pinBoton = 13;
const unsigned long debounceDelay = 50; // ms

// Estado del botón
int lastButtonState = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
// Buffer para entrada Serial por línea
String serialBuffer = "";
// Modo de ajustes
bool inSettings = false;
String pendingSetting = ""; // indica qué ajuste está esperando valor

// WiFi / Hora / Geolocalización para amanecer/anochecer
char wifiSsid[64] = "";
char wifiPass[64] = "";
// Preferencias para persistencia
Preferences prefs;

// MQTT
char mqttHost[128] = "";
int mqttPort = 1883;
char mqttTopic[128] = "iluminacion";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
WebServer webServer(80);
double locationLat = 0.0;
double locationLon = 0.0;
int timezoneOffsetHours = 0; // horas respecto a UTC
bool autoSunEnabled = false; // si true, sincroniza y activa modos en sunrise/sunset

// Sunrise/sunset calculados (minutos desde medianoche local)
int sunriseMinutes = -1;
int sunsetMinutes = -1;
int lastSunCalcDay = -1; // día del año cuando calculamos últimos valores
bool sunriseTriggeredToday = false;
bool sunsetTriggeredToday = false;

// Helpers (deg/rad)
static double degToRad(double deg) { return deg * M_PI / 180.0; }
static double radToDeg(double rad) { return rad * 180.0 / M_PI; }

// Calcula sunrise/sunset en minutos locales usando algoritmo aproximado (NOAA)
void calculateSunriseSunset(int year, int month, int day, double lat, double lon, int tzHours, int &sunriseMin, int &sunsetMin) {
  // día del año
  tm timeinfo = {};
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  mktime(&timeinfo);
  int dayOfYear = timeinfo.tm_yday + 1;

  double gamma = 2.0 * M_PI / 365.0 * (dayOfYear - 1 + ((12 - tzHours) / 24.0));

  double eqTime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma) - 0.014615 * cos(2*gamma) - 0.040849 * sin(2*gamma));
  double solarDec = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma) - 0.006758 * cos(2*gamma) + 0.000907 * sin(2*gamma) - 0.002697 * cos(3*gamma) + 0.00148 * sin(3*gamma);

  double latRad = degToRad(lat);
  double haArg = (cos(degToRad(90.833)) / (cos(latRad) * cos(solarDec)) - tan(latRad) * tan(solarDec));
  if (haArg > 1.0 || haArg < -1.0) {
    // Polar day/night, set as disabled
    sunriseMin = -1;
    sunsetMin = -1;
    return;
  }
  double hourAngle = acos(haArg);

  double solarNoon = 720.0 - 4.0 * lon - eqTime + tzHours * 60.0;
  double delta = radToDeg(hourAngle) * 4.0; // minutes

  double sunrise = solarNoon - delta;
  double sunset = solarNoon + delta;

  sunriseMin = (int)round(sunrise);
  sunsetMin = (int)round(sunset);
}

// Conectar a WiFi (bloqueante corto) — con depuración y limpieza previa
bool connectWiFi(const char* ssid, const char* pass, unsigned long timeoutMs = 10000) {
  if (strlen(ssid) == 0) {
    Serial.println(F("[WIFI] SSID vacío, no se intenta conectar"));
    return false;
  }
  Serial.print(F("[WIFI] Intentando conectar a SSID: "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  // forzar desconexión previa y limpiar estado
  WiFi.disconnect(true);
  delay(200);

  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  int lastStatus = -1;
  while ((millis() - start) < timeoutMs) {
    int s = WiFi.status();
    if (s != lastStatus) {
      lastStatus = s;
      Serial.print(F("[WIFI] status="));
      Serial.println(s);
    }
    if (s == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      Serial.print(F("[WIFI] Conectado, IP: "));
      Serial.println(ip);
      
      // Iniciar mDNS
      if (MDNS.begin(mdnsName)) {
        Serial.print(F("[mDNS] Iniciado: http://"));
        Serial.print(mdnsName);
        Serial.println(F(".local"));
        MDNS.addService("http", "tcp", 80);
      } else {
        Serial.println(F("[mDNS] Error al iniciar mDNS"));
      }
      
      // persistir credenciales
      prefs.begin("ilum", false);
      prefs.putString("ssid", String(ssid));
      prefs.putString("pass", String(pass));
      prefs.end();
      // sincronizar tiempo y calcular sunrise/sunset inmediatamente
      if (syncTimeAndCalculateSun()) {
        Serial.println(F("[WIFI] Hora sincronizada y sunrise/sunset calculados."));
      } else {
        Serial.println(F("[WIFI] No se pudo sincronizar la hora inmediatamente."));
      }
      // comprobar triggers inmediatos (si auto está activado)
      checkSunTriggersNow();
      // aplicar inmediatamente modo Día/Noche en base a la hora/sunrise-sunset
      applyDayNightBasedOnSun();
      return true;
    }
    delay(200);
  }

  int finalStatus = WiFi.status();
  Serial.print(F("[WIFI] No se pudo conectar. status="));
  Serial.println(finalStatus);
  const char* reason = "Desconocido";
  switch (finalStatus) {
    case WL_NO_SSID_AVAIL: reason = "SSID no disponible"; break;
    case WL_SCAN_COMPLETED: reason = "Escaneo completado (no conectado)"; break;
    case WL_CONNECT_FAILED: reason = "Fallo autenticación/conexión"; break;
    case WL_CONNECTION_LOST: reason = "Conexión perdida"; break;
    case WL_DISCONNECTED: reason = "Desconectado"; break;
    case WL_IDLE_STATUS: reason = "Idle/esperando"; break;
    default: break;
  }
  Serial.print(F("[WIFI] Razón aproximada: "));
  Serial.println(reason);
  return false;
}

// Forzar sincronización NTP y calcular sunrise/sunset para hoy
bool syncTimeAndCalculateSun() {
  if (!WiFi.isConnected()) {
    if (!connectWiFi(wifiSsid, wifiPass, 8000)) return false;
  }
  // configurar timezone offset en segundos
  long tzSecs = timezoneOffsetHours * 3600L;
  configTime(tzSecs, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) return false;
  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;
  calculateSunriseSunset(year, month, day, locationLat, locationLon, timezoneOffsetHours, sunriseMinutes, sunsetMinutes);
  lastSunCalcDay = timeinfo.tm_yday + 1;
  sunriseTriggeredToday = false;
  sunsetTriggeredToday = false;
  Serial.print(F("[SYNC] Sunrise (min): "));
  Serial.print(sunriseMinutes);
  Serial.print(F("  Sunset (min): "));
  Serial.println(sunsetMinutes);
  return true;
}

// Estados de modo
enum Modo { MODO_DIA = 0, MODO_AMANECER = 1, MODO_NOCHE = 2, MODO_TORMENTA = 3, MODO_ANOCHECER = 4 };
Modo modoActual = MODO_NOCHE; // por defecto: modo día

const char* modoNames[] = { "Día", "Amanecer", "Noche", "Tormenta", "Anochecer" };

// prototipos relacionados con MQTT/HA discovery
void publishDiscovery();
int nameToModo(const String &name);

// Escritura segura (clamp 0..255) con optimización para evitar escrituras innecesarias
static int lastValues[4] = {-1, -1, -1, -1}; // cache de últimos valores por canal
void safeLedcWrite(int canal, int valor) {
  if (valor < 0) valor = 0;
  if (valor > 255) valor = 255;
  // Solo escribir si el valor cambió
  if (lastValues[canal] != valor) {
    ledcWrite(canal, valor);
    lastValues[canal] = valor;
  }
}

// --- Estado y variables para modo tormenta (no bloqueante)
unsigned long stormNextEvent = 0; // siguiente evento (burst) cuando no hay burst en curso
unsigned long nextFlashTime = 0;  // tiempo para el siguiente flash dentro de un burst
unsigned long flashEndTime = 0;   // fin del flash actual
bool inFlash = false;             // indicador de que hay un burst en curso
bool flashActive = false;         // indicador de que un flash está activo
int burstCount = 0;
int burstIndex = 0;
int flashColor = 0; // 0 = blanco, 1 = azul
int flashDuration = 0;
// Niveles base durante la tormenta (suaves)
const int stormBaseWhite = 0;  // mínimo blanco durante tormenta
const int stormBaseBlue = 5;   // mínimo azul durante tormenta

// --- Duración compartida para transiciones (amanecer y anochecer)
unsigned long transitionDuration = 900UL * 1000UL; // duración por defecto: 900s (15 minutos) para simular amanecer/atardecer real

// --- Estado y variables para modo amanecer (no bloqueante)
unsigned long amanecerStart = 0;
bool amanecerActive = false;
const int amanecerTargetWhite = 255;
const int amanecerTargetOrange = 255;
const int amanecerTargetWhite2 = 255;

// --- Estado y variables para modo anochecer (no bloqueante)
unsigned long anochecerStart = 0;
bool anochecerActive = false;
const int anochecerStartWhite = 255;
const int anochecerStartOrange = 255;
const int anochecerStartWhite2 = 255;
const int anochecerTargetWhite = 0;
const int anochecerTargetOrange = 0;
const int anochecerTargetWhite2 = 0;
const int anochecerTargetBlue = 5; // azul sube levemente al anochecer

void setup() {
  Serial.begin(115200);
  
  // Deshabilitar watchdog para evitar resets durante operaciones largas
  disableCore0WDT();
  
  // Configurar PWM para cada pin con mayor prioridad
  ledcSetup(canalBlanco1, frecuencia, resolucion);
  ledcAttachPin(pinBlanco1, canalBlanco1);

  ledcSetup(canalNaranja, frecuencia, resolucion);
  ledcAttachPin(pinNaranja, canalNaranja);

  ledcSetup(canalAzul, frecuencia, resolucion);
  ledcAttachPin(pinAzul, canalAzul);

  ledcSetup(canalBlanco2, frecuencia, resolucion);
  ledcAttachPin(pinBlanco2, canalBlanco2);

  // Configurar botón con pull-up y activar modo día por defecto
  pinMode(pinBoton, INPUT_PULLUP);
  modoActual = MODO_NOCHE;
  randomSeed(analogRead(0));
  // cargar credenciales guardadas (si existen)
  loadSavedWiFi();
  loadMQTTConfig();
  // cargar ubicación guardada
  prefs.begin("ilum", true);
  locationLat = prefs.getDouble("lat", 0.0);
  locationLon = prefs.getDouble("lon", 0.0);
  timezoneOffsetHours = prefs.getInt("tz", 0);
  prefs.end();
  if (strlen(wifiSsid) > 0) {
    Serial.print(F("[BOOT] Credenciales WiFi encontradas: "));
    Serial.println(wifiSsid);
    if (connectWiFi(wifiSsid, wifiPass, 8000)) {
      Serial.println(F("[BOOT] WiFi conectado (guardado)."));
      // iniciar servidor web
      setupWebServer();
      // intentar MQTT si configurado
      if (strlen(mqttHost) > 0) connectMQTT();
      // sincronizar tiempo al arranque
      syncTimeAndCalculateSun();
      // aplicar modo día/noche inmediatamente tras sincronizar
      applyDayNightBasedOnSun();
    } else {
      Serial.println(F("[BOOT] Falló la conexión con credenciales guardadas."));
      modoDia();
    }
  } else {
    modoDia();
  }
  // Mostrar el formulario/menú por Serial para pruebas
  showMenu();
}

void loop() {
  // Leer botón (INPUT_PULLUP, activo en LOW)
  int lectura = digitalRead(pinBoton);
  if (lectura != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lectura != buttonState) {
      buttonState = lectura;
          if (buttonState == LOW) {
            // Ciclar entre modos: Día -> Amanecer -> Noche -> Tormenta -> Anochecer -> Día
            modoActual = (Modo)((modoActual + 1) % 5);
            if (modoActual == MODO_DIA) {
              modoDia();
              Serial.println("Modo Dia activado");
            } else if (modoActual == MODO_AMANECER) {
              enterAmanecer();
              Serial.println("Modo Amanecer activado");
            } else if (modoActual == MODO_NOCHE) {
              modoNoche();
              Serial.println("Modo Noche activado");
            } else if (modoActual == MODO_TORMENTA) {
              enterStorm();
              Serial.println("Modo Tormenta activado");
            } else {
              enterAnochecer();
              Serial.println("Modo Anochecer activado");
            }
          }
    }
  }
  lastButtonState = lectura;

  // Si estamos en modo tormenta o amanecer, ejecutar su manejador no bloqueante
  if (modoActual == MODO_TORMENTA) {
    handleStorm();
  } else if (modoActual == MODO_AMANECER) {
    handleAmanecer();
  } else if (modoActual == MODO_ANOCHECER) {
    handleAnochecer();
  }

  // manejar MQTT
  handleMQTT();

  // manejar peticiones HTTP del servidor web
  webServer.handleClient();

  // Leer comandos del formulario Serial
  handleSerialInput();
  
  // Revisión periódica para activar amanecer/anochecer automáticamente
  static unsigned long lastSunCheck = 0;
  unsigned long now = millis();
  if ((now - lastSunCheck) > 60000) { // cada minuto
    lastSunCheck = now;
    if (autoSunEnabled) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 2000)) {
        int today = timeinfo.tm_yday + 1;
        // si cambió el día, recalcular
        if (today != lastSunCalcDay) {
          syncTimeAndCalculateSun();
        }
        if (sunriseMinutes >= 0 && sunsetMinutes >= 0) {
          int curMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
          if (!sunriseTriggeredToday && curMin >= sunriseMinutes) {
            sunriseTriggeredToday = true;
            modoActual = MODO_AMANECER;
            enterAmanecer();
            Serial.println(F("[AUTO] Amanecer activado"));
          }
          if (!sunsetTriggeredToday && curMin >= sunsetMinutes) {
            sunsetTriggeredToday = true;
            modoActual = MODO_ANOCHECER;
            enterAnochecer();
            Serial.println(F("[AUTO] Anochecer activado"));
          }
        }
      }
    }
  }
  
  // Pequeño delay para estabilizar el loop y evitar saturación del CPU
  delay(1);
}

// MQTT loop/reconnect handling con límite de tiempo
void handleMQTT() {
  if (strlen(mqttHost) == 0) return;
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      if (connectMQTT()) lastMqttReconnectAttempt = 0;
    }
  } else {
    // Limitar el tiempo de procesamiento MQTT para no interferir con PWM
    mqttClient.loop();
  }
}

// 🎨 Modo noche: tonos suaves
void modoNoche() {
  safeLedcWrite(canalBlanco1, 0);
  safeLedcWrite(canalNaranja, 0);
  safeLedcWrite(canalAzul, 5);
  safeLedcWrite(canalBlanco2, 0);
}

// 🌞 Modo día: brillo máximo
void modoDia() {
  safeLedcWrite(canalBlanco1, 255);
  safeLedcWrite(canalNaranja, 255);
  safeLedcWrite(canalAzul, 0);
  safeLedcWrite(canalBlanco2, 255);
}

// Inicializar estado de amanecer
void enterAmanecer() {
  amanecerStart = millis();
  amanecerActive = true;
  // empezar desde apagado para el efecto
  safeLedcWrite(canalBlanco1, 0);
  safeLedcWrite(canalNaranja, 0);
  safeLedcWrite(canalBlanco2, 0);
  safeLedcWrite(canalAzul, 0);
}

// Manejador no bloqueante del efecto amanecer (rampa lineal)
void handleAmanecer() {
  if (!amanecerActive) return;
  unsigned long now = millis();
  unsigned long elapsed = now - amanecerStart;
  float progress = (float)elapsed / (float)transitionDuration;
  if (progress >= 1.0f) {
    // finalizar amanecer y pasar a modo día
    amanecerActive = false;
    modoActual = MODO_DIA;
    modoDia();
    Serial.println(F("[AMANECER] Completado. Modo Día activado"));
    return;
  }

  // calcular valores intermedios
  int w = (int)(amanecerTargetWhite * progress);
  int o = (int)(amanecerTargetOrange * progress);
  int w2 = (int)(amanecerTargetWhite2 * progress);

  safeLedcWrite(canalBlanco1, w);
  safeLedcWrite(canalNaranja, o);
  safeLedcWrite(canalBlanco2, w2);
}

// Inicializar estado de anochecer
void enterAnochecer() {
  anochecerStart = millis();
  anochecerActive = true;
  // empezar desde valores de día
  safeLedcWrite(canalBlanco1, anochecerStartWhite);
  safeLedcWrite(canalNaranja, anochecerStartOrange);
  safeLedcWrite(canalBlanco2, anochecerStartWhite2);
  safeLedcWrite(canalAzul, 0);
}

// Manejador no bloqueante del efecto anochecer (rampa inversa)
void handleAnochecer() {
  if (!anochecerActive) return;
  unsigned long now = millis();
  unsigned long elapsed = now - anochecerStart;
  float progress = (float)elapsed / (float)transitionDuration;
  if (progress >= 1.0f) {
    // finalizar anochecer y pasar a modo noche
    anochecerActive = false;
    modoActual = MODO_NOCHE;
    modoNoche();
    Serial.println(F("[ANOCHECER] Completado. Modo Noche activado"));
    return;
  }

  // interpolación lineal desde valores de día hacia valores de noche
  float inv = 1.0f - progress;
  int w = (int)(anochecerStartWhite * inv + anochecerTargetWhite * progress);
  int o = (int)(anochecerStartOrange * inv + anochecerTargetOrange * progress);
  int w2 = (int)(anochecerStartWhite2 * inv + anochecerTargetWhite2 * progress);
  int b = (int)(0 * inv + anochecerTargetBlue * progress);

  safeLedcWrite(canalBlanco1, w);
  safeLedcWrite(canalNaranja, o);
  safeLedcWrite(canalBlanco2, w2);
  safeLedcWrite(canalAzul, b);
}

// Inicializar estado de tormenta
void enterStorm() {
  // establecer niveles base suaves
  safeLedcWrite(canalBlanco1, stormBaseWhite);
  safeLedcWrite(canalBlanco2, stormBaseWhite);
  safeLedcWrite(canalAzul, stormBaseBlue);
  safeLedcWrite(canalNaranja, 0);

  inFlash = false;
  flashActive = false;
  burstCount = 0;
  burstIndex = 0;
  stormNextEvent = millis() + random(500, 3000);
  nextFlashTime = 0;
  flashEndTime = 0;
}

// Manejador no bloqueante del efecto de tormenta
void handleStorm() {
  unsigned long now = millis();

  if (!inFlash) {
    if (now >= stormNextEvent) {
      // Iniciar un burst de rayos
      inFlash = true;
      burstCount = random(1, 4); // 1..3 flashes
      burstIndex = 0;
      nextFlashTime = now; // empezar inmediatamente
      flashActive = false;
    } else {
      return;
    }
  }

  // Dentro de un burst
  if (!flashActive) {
    if (now >= nextFlashTime) {
      // iniciar flash
      flashColor = random(0, 2); // 0=blanco,1=azul
      flashDuration = random(40, 220);
      int intensity = random(160, 255);

      if (flashColor == 0) {
        safeLedcWrite(canalBlanco1, intensity);
        safeLedcWrite(canalBlanco2, intensity);
      } else {
        safeLedcWrite(canalAzul, intensity);
      }

      flashEndTime = now + flashDuration;
      flashActive = true;
    }
  } else {
    // estamos en un flash
    if (now >= flashEndTime) {
      // terminar flash: volver a niveles base
      safeLedcWrite(canalBlanco1, stormBaseWhite);
      safeLedcWrite(canalBlanco2, stormBaseWhite);
      safeLedcWrite(canalAzul, stormBaseBlue);

      flashActive = false;
      burstIndex++;

      if (burstIndex >= burstCount) {
        // terminar burst
        inFlash = false;
        stormNextEvent = now + random(2000, 8000); // tiempo hasta siguiente burst
        return;
      } else {
        // pausa corta antes del siguiente flash dentro del burst
        nextFlashTime = now + random(80, 600);
      }
    }
  }
}

// ------------------ Menú Serial para pruebas ------------------
void showMenu() {
  Serial.println(F("---- FORMULARIO DE PRUEBAS ----"));
  Serial.println(F("Pulse:"));
  Serial.println(F("  1 -> Modo Día"));
  Serial.println(F("  2 -> Modo Noche"));
  Serial.println(F("  3 -> Modo Tormenta"));
  Serial.println(F("  4 -> Modo Amanecer"));
  Serial.println(F("  5 -> Modo Anochecer"));
  Serial.println(F("  h -> Mostrar este menú"));
  Serial.println(F("  i -> Mostrar IP y estado WiFi"));
  Serial.println(F("  s -> Menu Ajustes (configuración)"));
  Serial.println(F("Comandos de duración:"));
  Serial.println(F("  a<segundos> -> Ajustar duración Amanecer (ej. a60)"));
  Serial.println(F("  d<segundos> -> Ajustar duración Anochecer (ej. d600)"));
  Serial.println(F("Comandos de sincronización y ubicación:"));
  Serial.println(F("  w<ssid>,<pass> -> Configurar WiFi y conectar (ej. wMiRed,clave)"));
  Serial.println(F("  l<lat>,<lon>,<tz> -> Configurar ubicación y zona (ej. l-34.6,-58.4,-3)"));
  Serial.println(F("  sync -> Forzar sincronización NTP y cálculo sunrise/sunset"));
  Serial.println(F("  auto1 -> Activar sincronización automática (usa sunrise/sunset)"));
  Serial.println(F("  auto0 -> Desactivar sincronización automática"));
  Serial.println(F("-------------------------------"));
}

// Menú de ajustes interactivo
void showSettingsMenu() {
  inSettings = true;
  pendingSetting = "";
  Serial.println(F("---- MENU DE AJUSTES ----"));
  Serial.println(F("  a -> Configurar WiFi (ssid,pass)"));
  Serial.println(F("  b -> Configurar MQTT (host,port,topic)"));
  Serial.println(F("  c -> Configurar ubicación (lat,lon,tz)"));
  Serial.println(F("  d -> Ajustar duraciones (a<segundos> / d<segundos>)"));
  Serial.println(F("  e -> Activar/desactivar auto (enviar A1/A0)"));
  Serial.println(F("  f -> Mostrar configuración actual"));
  Serial.println(F("  g -> Borrar preferencias guardadas"));
  Serial.println(F("  x -> Salir del menú de ajustes"));
  Serial.println(F("------------------------"));
}

void handleSerialInput() {
  if (!Serial) return;
  while (Serial.available()) {
    char c = Serial.read();
    // construir línea hasta salto de línea
    if (c == '\r' || c == '\n') {
      if (serialBuffer.length() == 0) continue;
      String cmd = serialBuffer;
      serialBuffer = "";
      // Si estamos en el menú de ajustes, manejar flujo interactivo
      if (inSettings) {
        // si no hay un setting pendiente, interpretamos la selección
        if (pendingSetting.length() == 0) {
          if (cmd.length() >= 1) {
            char sel = cmd.charAt(0);
            if (sel == 'a' || sel == 'A') {
              pendingSetting = "wifi";
              Serial.println(F("[SET] Enviar: ssid,pass (ej: MiFibra,clave)"));
            } else if (sel == 'b' || sel == 'B') {
              pendingSetting = "mqtt";
              Serial.println(F("[SET] Enviar: host,port,topic (ej: broker.local,1883,iluminacion)"));
            } else if (sel == 'c' || sel == 'C') {
              pendingSetting = "location";
              Serial.println(F("[SET] Enviar: lat,lon,tz (ej: -34.6,-58.4,-3)"));
            } else if (sel == 'd' || sel == 'D') {
              pendingSetting = "durations";
              Serial.println(F("[SET] Enviar: a<segundos> para amanecer o d<segundos> para anochecer (ej: a60)"));
            } else if (sel == 'e' || sel == 'E') {
              Serial.println(F("[SET] Enviar A1 para activar o A0 para desactivar la automatización."));
            } else if (sel == 'f' || sel == 'F') {
              // mostrar configuración actual
              Serial.println(F("[SET] Configuración actual:"));
              Serial.print(F("  SSID: ")); Serial.println(wifiSsid);
              Serial.print(F("  MQTT host: ")); Serial.println(mqttHost);
              Serial.print(F("  MQTT port: ")); Serial.println(mqttPort);
              Serial.print(F("  MQTT topic: ")); Serial.println(mqttTopic);
              Serial.print(F("  Lat,Lon,TZ: ")); Serial.print(locationLat); Serial.print(F(",")); Serial.print(locationLon); Serial.print(F(",")); Serial.println(timezoneOffsetHours);
            } else if (sel == 'g' || sel == 'G') {
              // borrar preferencias
              prefs.begin("ilum", false);
              prefs.clear();
              prefs.end();
              Serial.println(F("[SET] Preferencias borradas."));
            } else if (sel == 'x' || sel == 'X') {
              inSettings = false;
              pendingSetting = "";
              Serial.println(F("[SET] Saliendo del menú de ajustes."));
              showMenu();
            } else {
              Serial.print(F("[SET] Selección inválida: "));
              Serial.println(cmd);
              showSettingsMenu();
            }
          }
        } else {
          // tenemos un setting pendiente, procesar el valor enviado
          if (pendingSetting == "wifi") {
            int comma = cmd.indexOf(',');
            if (comma > 0) {
              String s = cmd.substring(0, comma);
              String p = cmd.substring(comma + 1);
              s.toCharArray(wifiSsid, sizeof(wifiSsid));
              p.toCharArray(wifiPass, sizeof(wifiPass));
              Serial.println(F("[SET] Intentando conectar a WiFi con valores recibidos..."));
              if (connectWiFi(wifiSsid, wifiPass, 8000)) {
                Serial.println(F("[WIFI] Conectado."));
                prefs.begin("ilum", false);
                prefs.putString("ssid", String(wifiSsid));
                prefs.putString("pass", String(wifiPass));
                prefs.end();
              } else {
                Serial.println(F("[WIFI] No se pudo conectar con esos datos."));
              }
            } else {
              Serial.println(F("[SET] Formato inválido. Use ssid,pass"));
            }
            pendingSetting = "";
            showSettingsMenu();
          } else if (pendingSetting == "mqtt") {
            int c1 = cmd.indexOf(',');
            int c2 = cmd.indexOf(',', c1 + 1);
            if (c1 > 0 && c2 > c1) {
              String shost = cmd.substring(0, c1);
              String sport = cmd.substring(c1 + 1, c2);
              String stopic = cmd.substring(c2 + 1);
              shost.toCharArray(mqttHost, sizeof(mqttHost));
              mqttPort = sport.toInt();
              stopic.toCharArray(mqttTopic, sizeof(mqttTopic));
              Serial.print(F("[SET] MQTT guardado: ")); Serial.print(mqttHost); Serial.print(F(":")); Serial.print(mqttPort); Serial.print(F(" topic=")); Serial.println(mqttTopic);
              saveMQTTConfig();
              connectMQTT();
            } else {
              Serial.println(F("[SET] Formato MQTT inválido. Use host,port,topic"));
            }
            pendingSetting = "";
            showSettingsMenu();
          } else if (pendingSetting == "location") {
            int c1 = cmd.indexOf(',');
            int c2 = cmd.indexOf(',', c1 + 1);
            if (c1 > 0 && c2 > c1) {
              String slat = cmd.substring(0, c1);
              String slon = cmd.substring(c1 + 1, c2);
              String stz = cmd.substring(c2 + 1);
              locationLat = slat.toFloat();
              locationLon = slon.toFloat();
              timezoneOffsetHours = stz.toInt();
              Serial.print(F("[SET] Ubicación guardada: ")); Serial.print(locationLat); Serial.print(F(",")); Serial.print(locationLon); Serial.print(F(", TZ=")); Serial.println(timezoneOffsetHours);
            } else {
              Serial.println(F("[SET] Formato ubicación inválido. Use lat,lon,tz"));
            }
            pendingSetting = "";
            showSettingsMenu();
          } else if (pendingSetting == "durations") {
            // reutilizamos el parsing ya existente: aNNN o dNNN
            if (cmd.length() > 1) {
              char op2 = cmd.charAt(0);
              String rest2 = cmd.substring(1);
              bool allDigits2 = true;
              for (unsigned int i = 0; i < rest2.length(); ++i) if (!isDigit(rest2.charAt(i))) { allDigits2 = false; break; }
              if (allDigits2) {
                unsigned long seconds = (unsigned long)rest2.toInt();
                if (op2 == 'a' || op2 == 'A' || op2 == 'd' || op2 == 'D') {
                  transitionDuration = seconds * 1000UL;
                  Serial.print(F("[SET] Duracion transiciones (Amanecer/Anochecer) ajustada a ")); Serial.print(seconds); Serial.println(F(" s"));
                } else {
                  Serial.println(F("[SET] Operador inválido para duraciones."));
                }
              } else {
                Serial.println(F("[SET] Formato inválido para duraciones."));
              }
            } else {
              Serial.println(F("[SET] Envía a<segundos> o d<segundos>"));
            }
            pendingSetting = "";
            showSettingsMenu();
          } else {
            // otro caso: limpiar
            pendingSetting = "";
            showSettingsMenu();
          }
        }
        // no ejecutar el procesamiento normal cuando estamos en settings
        continue;
      }
      // procesar comando completo
      if (cmd == "1") {
        modoActual = MODO_NOCHE;
        modoDia();
        Serial.println(F("[CMD] Modo Día activado"));
      } else if (cmd == "2") {
        modoActual = MODO_NOCHE;
        modoNoche();
        Serial.println(F("[CMD] Modo Noche activado"));
      } else if (cmd == "3") {
        modoActual = MODO_TORMENTA;
        enterStorm();
        Serial.println(F("[CMD] Modo Tormenta activado"));
      } else if (cmd == "4") {
        modoActual = MODO_AMANECER;
        enterAmanecer();
        Serial.println(F("[CMD] Modo Amanecer activado"));
      } else if (cmd == "5") {
        modoActual = MODO_ANOCHECER;
        enterAnochecer();
        Serial.println(F("[CMD] Modo Anochecer activado"));
      } else if (cmd == "h" || cmd == "H") {
        showMenu();
      } else if (cmd == "i" || cmd == "I") {
        // Mostrar IP y estado WiFi
        Serial.println(F("==== INFO DE RED ===="));
        if (WiFi.isConnected()) {
          Serial.print(F("Estado WiFi: CONECTADO\n"));
          Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
          Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
          Serial.print(F("Acceso web (IP): http://")); Serial.println(WiFi.localIP());
          Serial.print(F("Acceso web (nombre): http://")); Serial.print(mdnsName); Serial.println(F(".local"));
          Serial.print(F("Gateway: ")); Serial.println(WiFi.gatewayIP());
          Serial.print(F("Máscara: ")); Serial.println(WiFi.subnetMask());
          Serial.print(F("MAC: ")); Serial.println(WiFi.macAddress());
          Serial.print(F("Intensidad señal: ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
        } else {
          Serial.println(F("Estado WiFi: DESCONECTADO"));
          Serial.println(F("Use comando w<ssid>,<pass> para conectar"));
        }
        Serial.println(F("===================="));
      } else if (cmd == "s" || cmd == "S") {
        showSettingsMenu();
      } else {
        // comandos de ajuste: 'a<segundos>' para amanecer, 'd<segundos>' para anochecer
        if (cmd.length() > 1) {
          char op = cmd.charAt(0);
          String rest = cmd.substring(1);
          // Durations: aNNN or dNNN
          if (op == 'a' || op == 'A' || op == 'd' || op == 'D') {
            bool allDigits = true;
            for (unsigned int i = 0; i < rest.length(); ++i) {
              if (!isDigit(rest.charAt(i))) { allDigits = false; break; }
            }
            if (allDigits) {
              unsigned long seconds = (unsigned long)rest.toInt();
              if (op == 'a' || op == 'A' || op == 'd' || op == 'D') {
                transitionDuration = seconds * 1000UL;
                Serial.print(F("[CMD] Duracion transiciones (Amanecer/Anochecer) ajustada a "));
                Serial.print(seconds);
                Serial.println(F(" s"));
              }
            } else {
              Serial.print(F("Comando inválido o formato incorrecto: "));
              Serial.println(cmd);
            }
          }
          // WiFi: wssid,pass
          else if (op == 'w' || op == 'W') {
            int comma = rest.indexOf(',');
            if (comma > 0) {
              String s = rest.substring(0, comma);
              String p = rest.substring(comma + 1);
              s.toCharArray(wifiSsid, sizeof(wifiSsid));
              p.toCharArray(wifiPass, sizeof(wifiPass));
              Serial.println(F("[CMD] Intentando conectar a WiFi..."));
              if (connectWiFi(wifiSsid, wifiPass, 8000)) {
                Serial.println(F("[WIFI] Conectado."));
                  // guardar credenciales
                  prefs.begin("ilum", false);
                  prefs.putString("ssid", String(wifiSsid));
                  prefs.putString("pass", String(wifiPass));
                  prefs.end();
                  // intentar sincronizar NTP y MQTT
                  syncTimeAndCalculateSun();
                  if (strlen(mqttHost) > 0) connectMQTT();
              } else {
                Serial.println(F("[WIFI] No se pudo conectar."));
              }
            } else {
              Serial.println(F("Formato WiFi inválido. Use w<ssid>,<pass>"));
            }
          }
           // MQTT config: m<host>,<port>,<topic>
           else if (op == 'm' || op == 'M') {
             int c1 = rest.indexOf(',');
             int c2 = rest.indexOf(',', c1 + 1);
             if (c1 > 0 && c2 > c1) {
               String shost = rest.substring(0, c1);
               String sport = rest.substring(c1 + 1, c2);
               String stopic = rest.substring(c2 + 1);
               shost.toCharArray(mqttHost, sizeof(mqttHost));
               mqttPort = sport.toInt();
               stopic.toCharArray(mqttTopic, sizeof(mqttTopic));
               Serial.print(F("[MQTT] Configurado host=")); Serial.print(mqttHost);
               Serial.print(F(" port=")); Serial.print(mqttPort);
               Serial.print(F(" topic=")); Serial.println(mqttTopic);
               saveMQTTConfig();
               connectMQTT();
             } else {
               Serial.println(F("Formato MQTT inválido. Use m<host>,<port>,<topic>"));
             }
           }
          // Location: llat,lon,tz
          else if (op == 'l' || op == 'L') {
            // formato: l<lat>,<lon>,<tz>
            int c1 = rest.indexOf(',');
            int c2 = rest.indexOf(',', c1 + 1);
            if (c1 > 0 && c2 > c1) {
              String slat = rest.substring(0, c1);
              String slon = rest.substring(c1 + 1, c2);
              String stz = rest.substring(c2 + 1);
              locationLat = slat.toFloat();
              locationLon = slon.toFloat();
              timezoneOffsetHours = stz.toInt();
              Serial.print(F("[LOC] Set lat=")); Serial.print(locationLat);
              Serial.print(F(" lon=")); Serial.print(locationLon);
              Serial.print(F(" tz=")); Serial.println(timezoneOffsetHours);
            } else {
              Serial.println(F("Formato ubicación inválido. Use l<lat>,<lon>,<tz>"));
            }
          }
          // sync command exact
          else if (rest.equalsIgnoreCase("ync") && (op == 's' || op == 'S')) {
            // whole command 'sync'
            if (syncTimeAndCalculateSun()) Serial.println(F("[SYNC] OK")); else Serial.println(F("[SYNC] Falló"));
          }
          // auto on/off: auto1 or auto0 -> we accept 'A1' or 'A0' or 'auto1' via 'a' prefix handled above; support explicit
          else if ((op == 'A' || op == 'a') && (rest == "1" || rest == "0")) {
            if (rest == "1") {
              autoSunEnabled = true;
              // intentar sincronizar inmediatamente
              if (syncTimeAndCalculateSun()) {
                Serial.println(F("[AUTO] Activado y sincronizado"));
              } else {
                Serial.println(F("[AUTO] Activado, sync falló"));
              }
              // aplicar modo Día/Noche inmediatamente (usa sunrise/sunset o fallback horario)
              applyDayNightBasedOnSun();
            } else {
              autoSunEnabled = false;
              Serial.println(F("[AUTO] Desactivado"));
            }
          }
          else {
            Serial.print(F("Comando desconocido: "));
            Serial.println(cmd);
          }
        } else {
          Serial.print(F("Comando desconocido: "));
          Serial.println(cmd);
        }
      }
    } else {
      serialBuffer += c;
      // limitar tamaño para evitar desbordes
      if (serialBuffer.length() > 32) serialBuffer = serialBuffer.substring(0,32);
    }
  }
}

// ---------------- Preferences helpers ----------------
void loadSavedWiFi() {
  prefs.begin("ilum", true);
  String s = prefs.getString("ssid", "");
  String p = prefs.getString("pass", "");
  prefs.end();
  s.toCharArray(wifiSsid, sizeof(wifiSsid));
  p.toCharArray(wifiPass, sizeof(wifiPass));
}

void saveMQTTConfig() {
  prefs.begin("ilum", false);
  prefs.putString("mhost", String(mqttHost));
  prefs.putInt("mport", mqttPort);
  prefs.putString("mtopic", String(mqttTopic));
  prefs.end();
}

void loadMQTTConfig() {
  prefs.begin("ilum", true);
  String mh = prefs.getString("mhost", "");
  int mp = prefs.getInt("mport", 1883);
  String mt = prefs.getString("mtopic", "iluminacion");
  prefs.end();
  mh.toCharArray(mqttHost, sizeof(mqttHost));
  mqttPort = mp;
  mt.toCharArray(mqttTopic, sizeof(mqttTopic));
}

// ---------------- MQTT helpers ----------------
void publishState() {
  if (!mqttClient.connected()) return;
  String payload = String(modoNames[(int)modoActual]);
  mqttClient.publish(String(mqttTopic).c_str(), payload.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print(F("[MQTT] Mensaje recibido en "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(msg);
  // aceptar comandos: nombres (Día, Amanecer, Noche, Tormenta, Anochecer) o '1'..'5'
  int m = -1;
  // si es número
  bool allDigits = true;
  for (unsigned int i = 0; i < msg.length(); ++i) if (!isDigit(msg.charAt(i))) { allDigits = false; break; }
  if (allDigits && msg.length() > 0) {
    int idx = msg.toInt();
    if (idx >= 1 && idx <= 5) m = idx - 1;
  } else {
    m = nameToModo(msg);
  }
  if (m >= 0 && m < 5) {
    modoActual = (Modo)m;
    switch (modoActual) {
      case MODO_DIA: modoDia(); Serial.println(F("[MQTT] Modo Día via MQTT")); break;
      case MODO_AMANECER: enterAmanecer(); Serial.println(F("[MQTT] Modo Amanecer via MQTT")); break;
      case MODO_NOCHE: modoNoche(); Serial.println(F("[MQTT] Modo Noche via MQTT")); break;
      case MODO_TORMENTA: enterStorm(); Serial.println(F("[MQTT] Modo Tormenta via MQTT")); break;
      case MODO_ANOCHECER: enterAnochecer(); Serial.println(F("[MQTT] Modo Anochecer via MQTT")); break;
    }
    publishState();
  }
}

bool connectMQTT() {
  if (strlen(mqttHost) == 0) return false;
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setCallback(mqttCallback);
  String clientId = "iluminacion-" + String(WiFi.macAddress());
  bool ok = mqttClient.connect(clientId.c_str());
  if (ok) {
    Serial.println(F("[MQTT] Conectado al broker"));
    // suscribirse a topic/command
    String cmdTopic = String(mqttTopic) + String("/set");
    mqttClient.subscribe(cmdTopic.c_str());
    // publicar discovery para Home Assistant
    publishDiscovery();
    publishState();
  } else {
    Serial.print(F("[MQTT] Conexión fallida, rc="));
    Serial.println(mqttClient.state());
  }
  return ok;
}

// Comprobar inmediatamente los triggers de sunrise/sunset y activar modos si corresponde
void checkSunTriggersNow() {
  if (!autoSunEnabled) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) return;
  int curMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  // reset triggers si cambiamos de día
  int today = timeinfo.tm_yday + 1;
  if (today != lastSunCalcDay) {
    // recalcular por si
    syncTimeAndCalculateSun();
  }
  if (sunriseMinutes >= 0 && sunsetMinutes >= 0) {
    if (!sunriseTriggeredToday && curMin >= sunriseMinutes) {
      sunriseTriggeredToday = true;
      modoActual = MODO_AMANECER;
      enterAmanecer();
      Serial.println(F("[AUTO] Amanecer activado (check inmediato tras conexión WiFi)"));
    }
    if (!sunsetTriggeredToday && curMin >= sunsetMinutes) {
      sunsetTriggeredToday = true;
      modoActual = MODO_ANOCHECER;
      enterAnochecer();
      Serial.println(F("[AUTO] Anochecer activado (check inmediato tras conexión WiFi)"));
    }
  }
}

// Aplicar modo Día o Noche inmediatamente en base a sunrise/sunset (si están disponibles)
// o en su defecto usar un fallback horario (7:00-19:00 día).
void applyDayNightBasedOnSun() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 3000)) {
    Serial.println(F("[AUTO] No se puede obtener hora local para decidir Día/Noche"));
    return;
  }

  int curMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  Serial.printf("[DEBUG] Hora actual en minutos: %d\n", curMin);
  Serial.printf("[DEBUG] Sunrise en minutos: %d, Sunset en minutos: %d\n", sunriseMinutes, sunsetMinutes);

  if (sunriseMinutes >= 0 && sunsetMinutes >= 0) {
    // Estamos entre amanecer y atardecer
    if (curMin >= sunriseMinutes && curMin < sunsetMinutes) {
      if (!sunriseTriggeredToday) {
        // Primera vez que detectamos el amanecer hoy: lanzar efecto
        sunriseTriggeredToday = true;
        modoActual = MODO_AMANECER;
        enterAmanecer();
        Serial.println(F("[AUTO] Hora coincide con amanecer -> Modo Amanecer"));
      } else {
        // Ya se disparó el amanecer hoy: si el efecto aún está en curso, mantenerlo;
        // si no, asegurarse de que estamos en MODO_DIA.
        if (amanecerActive) {
          // Nada que hacer: el manejador no bloqueante actualizará las luces
        } else {
          if (modoActual != MODO_DIA) {
            modoActual = MODO_DIA;
            modoDia();
            Serial.println(F("[AUTO] Ya amaneció, activando Modo Día"));
          }
        }
      }
    } else {
      // Estamos fuera del rango diurno
      if (curMin >= sunsetMinutes) {
        if (!sunsetTriggeredToday) {
          // Primera vez que detectamos el atardecer hoy: lanzar anochecer
          sunsetTriggeredToday = true;
          modoActual = MODO_ANOCHECER;
          enterAnochecer();
          Serial.println(F("[AUTO] Hora coincide con anochecer -> Modo Anochecer"));
        } else {
          if (modoActual != MODO_NOCHE && !anochecerActive) {
            modoActual = MODO_NOCHE;
            modoNoche();
            Serial.println(F("[AUTO] Ya anocheció, activando Modo Noche"));
          }
        }
      } else {
        // Antes del amanecer: asegurarse de estar en NOCHE
        if (modoActual != MODO_NOCHE) {
          modoActual = MODO_NOCHE;
          modoNoche();
          Serial.println(F("[AUTO] Antes del amanecer -> Modo Noche"));
        }
      }
    }
  } else {
    // Fallback sencillo si no hay sunrise/sunset: considerar día entre 07:00 y 19:00
    Serial.println(F("[DEBUG] Usando fallback horario (07:00-19:00)"));
    if (timeinfo.tm_hour >= 7 && timeinfo.tm_hour < 19) {
      modoActual = MODO_DIA;
      modoDia();
      Serial.println(F("[AUTO] Fallback horario -> Modo Día"));
    } else {
      modoActual = MODO_NOCHE;
      modoNoche();
      Serial.println(F("[AUTO] Fallback horario -> Modo Noche"));
    }
  }
}

int nameToModo(const String &s) {
  String lower = s;
  lower.toLowerCase();
  if (lower == "día" || lower == "dia") return 0;
  if (lower == "amanecer") return 1;
  if (lower == "noche") return 2;
  if (lower == "tormenta") return 3;
  if (lower == "anochecer") return 4;
  return -1;
}

void publishDiscovery() {
  if (!mqttClient.connected()) return;
  // topic de discovery: homeassistant/select/<object_id>/config
  String objectId = String(mqttTopic);
  // normalizar barras y caracteres no permitidos en object id
  for (unsigned int i = 0; i < objectId.length(); ++i) if (objectId.charAt(i) == '/') objectId.setCharAt(i, '_');
  String discTopic = String("homeassistant/select/") + objectId + String("/config");
  // payload JSON
  String payload = "{";
  payload += "\"name\":\"Iluminacion\",";
  payload += "\"unique_id\":\"iluminacion_" + objectId + "\",";
  payload += "\"command_topic\":\"" + String(mqttTopic) + "/set\",";
  payload += "\"state_topic\":\"" + String(mqttTopic) + "\",";
  payload += "\"options\":[\"Día\",\"Amanecer\",\"Noche\",\"Tormenta\",\"Anochecer\"]";
  payload += "}";
  mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
  Serial.print(F("[MQTT] Discovery publicado en: "));
  Serial.println(discTopic);
}

// ========== SERVIDOR WEB ==========

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Iluminación Paladarium</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
  html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px}";
  html += "h2{color:#555;margin-top:30px}";
  html += ".btn{display:inline-block;padding:10px 20px;margin:10px 5px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;border:none;cursor:pointer}";
  html += ".btn:hover{background:#45a049}";
  html += ".info{background:#e7f3fe;padding:10px;border-left:4px solid #2196F3;margin:10px 0}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🌿 Iluminación Paladarium</h1>";
  html += "<div class='info'><strong>Acceso:</strong> http://" + String(mdnsName) + ".local</div>";
  html += "<div class='info'><strong>IP:</strong> " + WiFi.localIP().toString() + "</div>";
  html += "<div class='info'><strong>Modo actual:</strong> " + String(modoNames[(int)modoActual]) + "</div>";
  html += "<h2>Menú</h2>";
  html += "<a href='/config' class='btn'>⚙️ Configuración</a>";
  html += "<a href='/status' class='btn'>📊 Estado del sistema</a>";
  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleConfig() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Configuración</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
  html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
  html += "h1,h2{color:#333}input,select{width:100%;padding:8px;margin:5px 0 15px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}";
  html += "label{font-weight:bold;color:#555}.btn{display:inline-block;padding:10px 20px;margin:10px 5px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;border:none;cursor:pointer}";
  html += ".btn:hover{background:#45a049}.back{background:#999}.back:hover{background:#888}</style></head><body>";
  html += "<div class='container'><h1>⚙️ Configuración</h1>";
  
  html += "<form method='POST' action='/config/save'>";
  html += "<h2>MQTT</h2>";
  html += "<label>Broker IP/Host:</label><input type='text' name='mqtt_host' value='" + String(mqttHost) + "' placeholder='192.168.1.100'>";
  html += "<label>Puerto:</label><input type='number' name='mqtt_port' value='" + String(mqttPort) + "' placeholder='1883'>";
  html += "<label>Topic:</label><input type='text' name='mqtt_topic' value='" + String(mqttTopic) + "' placeholder='iluminacion'>";
  
  html += "<h2>Ubicación (para amanecer/anochecer)</h2>";
  html += "<label>Latitud:</label><input type='text' name='lat' value='" + String(locationLat, 4) + "' placeholder='-34.6037'>";
  html += "<label>Longitud:</label><input type='text' name='lon' value='" + String(locationLon, 4) + "' placeholder='-58.3816'>";
  html += "<label>Zona horaria (UTC offset en horas):</label><input type='number' name='tz' value='" + String(timezoneOffsetHours) + "' placeholder='-3'>";
  
  html += "<h2>Duración de efectos (segundos)</h2>";
  html += "<label>Duración Amanecer/Anochecer:</label><input type='number' name='dur_transition' value='" + String(transitionDuration / 1000) + "' placeholder='60'><small style='color:#666'>Ambos efectos usan la misma duración</small>";
  
  html += "<button type='submit' class='btn'>💾 Guardar configuración</button>";
  html += "</form>";
  html += "<br><a href='/' class='btn back'>🏠 Volver</a>";
  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleConfigSave() {
  if (webServer.hasArg("mqtt_host")) {
    String host = webServer.arg("mqtt_host");
    host.toCharArray(mqttHost, sizeof(mqttHost));
  }
  if (webServer.hasArg("mqtt_port")) {
    mqttPort = webServer.arg("mqtt_port").toInt();
  }
  if (webServer.hasArg("mqtt_topic")) {
    String topic = webServer.arg("mqtt_topic");
    topic.toCharArray(mqttTopic, sizeof(mqttTopic));
  }
  if (webServer.hasArg("lat")) {
    locationLat = webServer.arg("lat").toFloat();
  }
  if (webServer.hasArg("lon")) {
    locationLon = webServer.arg("lon").toFloat();
  }
  if (webServer.hasArg("tz")) {
    timezoneOffsetHours = webServer.arg("tz").toInt();
  }
  if (webServer.hasArg("dur_transition")) {
    transitionDuration = webServer.arg("dur_transition").toInt() * 1000UL;
  }
  
  // Guardar configuración MQTT
  saveMQTTConfig();
  
  // Guardar ubicación en preferencias
  prefs.begin("ilum", false);
  prefs.putDouble("lat", locationLat);
  prefs.putDouble("lon", locationLon);
  prefs.putInt("tz", timezoneOffsetHours);
  prefs.end();
  
  Serial.println(F("[WEB] Configuración guardada"));
  
  // Reconectar MQTT si está configurado
  if (strlen(mqttHost) > 0) {
    connectMQTT();
  }
  
  // Recalcular sunrise/sunset
  syncTimeAndCalculateSun();
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='3;url=/'>";
  html += "<style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background:#f0f0f0}";
  html += ".msg{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);max-width:400px;margin:0 auto}";
  html += "h1{color:#4CAF50}</style></head><body>";
  html += "<div class='msg'><h1>✅ Configuración guardada</h1><p>Redirigiendo...</p></div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleStatus() {
  struct tm timeinfo;
  String currentTime = "No sincronizada";
  if (getLocalTime(&timeinfo, 1000)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    currentTime = String(timeStr);
  }
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Estado del sistema</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
  html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px}";
  html += "table{width:100%;border-collapse:collapse;margin:20px 0}";
  html += "td{padding:10px;border-bottom:1px solid #ddd}td:first-child{font-weight:bold;width:40%;color:#555}";
  html += ".btn{display:inline-block;padding:10px 20px;margin:10px 5px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px}";
  html += ".btn:hover{background:#45a049}.back{background:#999}.back:hover{background:#888}";
  html += ".status-ok{color:green}. status-error{color:red}</style></head><body>";
  html += "<div class='container'><h1>📊 Estado del sistema</h1>";
  
  html += "<table>";
  html += "<tr><td>Modo actual</td><td>" + String(modoNames[(int)modoActual]) + "</td></tr>";
  html += "<tr><td>WiFi</td><td class='status-ok'>✅ Conectado</td></tr>";
  html += "<tr><td>IP</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>SSID</td><td>" + String(wifiSsid) + "</td></tr>";
  html += "<tr><td>MQTT</td><td>" + String(mqttClient.connected() ? "<span class='status-ok'>✅ Conectado</span>" : "<span class='status-error'>❌ Desconectado</span>") + "</td></tr>";
  html += "<tr><td>Broker MQTT</td><td>" + String(mqttHost) + ":" + String(mqttPort) + "</td></tr>";
  html += "<tr><td>Topic MQTT</td><td>" + String(mqttTopic) + "</td></tr>";
  html += "<tr><td>Hora actual</td><td>" + currentTime + "</td></tr>";
  html += "<tr><td>Sunrise (amanecer)</td><td>" + (sunriseMinutes >= 0 ? String(sunriseMinutes / 60) + ":" + String(sunriseMinutes % 60) : "No calculado") + "</td></tr>";
  html += "<tr><td>Sunset (atardecer)</td><td>" + (sunsetMinutes >= 0 ? String(sunsetMinutes / 60) + ":" + String(sunsetMinutes % 60) : "No calculado") + "</td></tr>";
  html += "<tr><td>Ubicación</td><td>Lat: " + String(locationLat, 4) + ", Lon: " + String(locationLon, 4) + ", TZ: UTC" + String(timezoneOffsetHours > 0 ? "+" : "") + String(timezoneOffsetHours) + "</td></tr>";
  html += "<tr><td>Automatización</td><td>" + String(autoSunEnabled ? "✅ Activada" : "❌ Desactivada") + "</td></tr>";
  html += "</table>";
  
  html += "<a href='/' class='btn back'>🏠 Volver</a>";
  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleNotFound() {
  webServer.send(404, "text/plain", "404: Página no encontrada");
}

void setupWebServer() {
  webServer.on("/", handleRoot);
  webServer.on("/config", handleConfig);
  webServer.on("/config/save", HTTP_POST, handleConfigSave);
  webServer.on("/status", handleStatus);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println(F("[WEB] Servidor web iniciado en puerto 80"));
  Serial.print(F("[WEB] Accede a: http://"));
  Serial.print(mdnsName);
  Serial.print(F(".local o http://"));
  Serial.println(WiFi.localIP());
}
