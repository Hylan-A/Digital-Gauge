//=====================================================================
// Project: Digital Pressure Gauge with OTA
// File: gauge_finishdesign1_OTA_watchdog.ino
//
// Adds:
// - 15-second ESP32 task watchdog
// - Non-blocking Wi-Fi and MQTT recovery
// - Display initialization before network connection
// - MQTT publishing only while connected
//=====================================================================

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "Home-IoT";
const char* password = "1167Wedwards";

#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  3
#define TFT_MOSI 10
#define TFT_SCLK 8

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

const int cx = 120;
const int cy = 120;
const int radius = 120;
const int needleLength = 98;
const int sensorPin = 2;

const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long GAUGE_UPDATE_MS = 250;
const unsigned long MQTT_PUBLISH_MS = 1000;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastGaugeUpdate = 0;
unsigned long lastMqttPublish = 0;

float oldNeedlePsi = 0.0;
float currentPsi = 0.0;

void drawTicks() {
  for (int psi = 0; psi <= 100; psi += 5) {
    float angleDeg = 45.0 - (psi * 270.0 / 100.0);
    float angleRad = angleDeg * DEG_TO_RAD;
    int x1, y1, x2, y2;

    if (psi % 25 == 0) {
      x1 = cx + cos(angleRad) * (radius - 20);
      y1 = cy + sin(angleRad) * (radius - 20);
      x2 = cx + cos(angleRad) * radius;
      y2 = cy + sin(angleRad) * radius;
      tft.drawLine(x1, y1, x2, y2, GC9A01A_WHITE);
      tft.drawLine(x1 + 1, y1, x2 + 1, y2, GC9A01A_WHITE);
      tft.drawLine(x1 - 1, y1, x2 - 1, y2, GC9A01A_WHITE);
    } else {
      x1 = cx + cos(angleRad) * (radius - 15);
      y1 = cy + sin(angleRad) * (radius - 15);
      x2 = cx + cos(angleRad) * radius;
      y2 = cy + sin(angleRad) * radius;
      tft.drawLine(x1, y1, x2, y2, GC9A01A_DARKGREY);
      tft.drawLine(x1 + 1, y1, x2 + 1, y2, GC9A01A_DARKGREY);
      tft.drawLine(x1 - 1, y1, x2 - 1, y2, GC9A01A_DARKGREY);
    }
  }
}

void drawLabels() {
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(50, 175);  tft.print("0");
  tft.setCursor(34, 80);   tft.print("25");
  tft.setCursor(109, 28);  tft.print("50");
  tft.setCursor(182, 80);  tft.print("75");
  tft.setCursor(165, 175); tft.print("100");
}

void drawColorArc() {
  for (int psi = 0; psi <= 100; psi++) {
    uint16_t color;
    if (psi < 80) color = GC9A01A_GREEN;
    else if (psi < 90) color = GC9A01A_ORANGE;
    else color = GC9A01A_RED;

    float angleRad = (135.0 + (psi * 270.0 / 100.0)) * DEG_TO_RAD;
    int x = cx + cos(angleRad) * (radius - 3);
    int y = cy + sin(angleRad) * (radius - 3);
    tft.fillCircle(x, y, 2, color);
  }
}

void drawNeedle(float psi, uint16_t color) {
  float angleRad = (135.0 + (psi * 270.0 / 100.0)) * DEG_TO_RAD;
  int x = cx + cos(angleRad) * needleLength;
  int y = cy + sin(angleRad) * needleLength;
  tft.drawLine(cx, cy, x, y, color);
  tft.drawLine(cx + 1, cy, x + 1, y, color);
  tft.drawLine(cx - 1, cy, x - 1, y, color);
  tft.fillCircle(cx, cy, 4, GC9A01A_WHITE);
}

void drawPressureValue(float psi) {
  tft.fillRect(50, 200, 130, 40, GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_CYAN);
  tft.setTextSize(3);
  tft.setCursor(85, 200);
  tft.print(psi, 1);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(3);
  tft.setCursor(95, 75);
  tft.print("PSI");
}

void initializeDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setDataMode(SPI_MODE3);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);
  drawColorArc();
  drawTicks();
  drawLabels();
  drawNeedle(0.0, GC9A01A_RED);
  drawPressureValue(0.0);
}

void initializeWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config = {};
  config.timeout_ms = 15000;
  config.idle_core_mask = 1;
  config.trigger_panic = true;

  esp_err_t result = esp_task_wdt_init(&config);
  if (result == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&config);
  }
#else
  esp_task_wdt_init(15, true);
#endif
  esp_task_wdt_add(NULL);
  Serial.println("15-second watchdog active");
}

void startWiFi() {
  Serial.println("Starting Wi-Fi connection");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  lastWiFiAttempt = millis();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastWiFiAttempt >= WIFI_RETRY_MS) {
    Serial.println("Wi-Fi disconnected; retrying");
    startWiFi();
  }
}

void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (client.connected()) {
    client.loop();
    return;
  }

  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = millis();

  Serial.print("Attempting MQTT connection...");
  if (client.connect("ESP32Gauge", "myuser", "2N3904")) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

float readPressure() {
  long total = 0;
  for (int i = 0; i < 64; i++) {
    total += analogRead(sensorPin);
    delay(2);
    esp_task_wdt_reset();
  }

  int adc = total / 64;
  float psi = (adc - 680) * 100.0 / (3260 - 680);
  Serial.println(adc);
  return psi;
}

void updateGauge() {
  currentPsi = readPressure();

  // Keep the physical needle on the printed 0-100 PSI scale while
  // retaining the unmodified calibrated value for the digital display/MQTT.
  float oldDisplayPsi = constrain(oldNeedlePsi, 0.0f, 100.0f);
  float newDisplayPsi = constrain(currentPsi, 0.0f, 100.0f);

  drawNeedle(oldDisplayPsi, GC9A01A_BLACK);
  drawLabels();
  tft.fillCircle(cx, cy, 6, GC9A01A_WHITE);
  drawNeedle(newDisplayPsi, GC9A01A_RED);
  drawPressureValue(currentPsi);
  oldNeedlePsi = currentPsi;
}

void setupOTA() {
  ArduinoOTA.setHostname("IrrigationGauge");
  ArduinoOTA.onStart([]() { Serial.println("OTA Update Started"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA Update Complete"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // The local gauge starts even if the network is unavailable.
  initializeDisplay();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  startWiFi();

  client.setServer("192.168.1.218", 1883);
  client.setSocketTimeout(2);

  setupOTA();
  initializeWatchdog();
  Serial.println("Startup Complete");
}

void loop() {
  esp_task_wdt_reset();
  ArduinoOTA.handle();
  maintainWiFi();
  maintainMqtt();

  unsigned long now = millis();

  if (now - lastGaugeUpdate >= GAUGE_UPDATE_MS) {
    lastGaugeUpdate = now;
    updateGauge();
  }

  if (client.connected() && now - lastMqttPublish >= MQTT_PUBLISH_MS) {
    lastMqttPublish = now;
    char msg[12];
    dtostrf(currentPsi, 1, 1, msg);
    client.publish("shop/pressure", msg);
  }

  delay(5);
}
