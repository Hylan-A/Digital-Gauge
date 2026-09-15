//=====================================================================
// Project: Digital Pressure Gauge with OTA
// File: gauge_finishdesign1_OTA.ino
//
// Author: Hylan
// Date: July 28, 2026
//
// Purpose:
// This sketch reads an analog pressure sensor, converts the ADC value
// into PSI, displays the result on a round GC9A01A TFT gauge, publishes
// the pressure value over MQTT, and supports OTA firmware updates.
//
// Features:
// - ESP32 Wi-Fi connection
// - MQTT publishing
// - Arduino OTA updates
// - Averaged ADC readings for smoother display
// - Custom gauge face with color bands, tick marks, and needle
//
// Hardware:
// - ESP32
// - Analog pressure transducer
// - GC9A01A round TFT display
// - Wi-Fi network access
// - MQTT broker
//=====================================================================

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

//==============================================================
// Wi-Fi and MQTT objects
//==============================================================

WiFiClient espClient;
PubSubClient client(espClient);

//==============================================================
// Network credentials
// NOTE: For a shared repo, move these to a secrets file.
//==============================================================

const char* ssid = "Home-IoT";
const char* password = "1167Wedwards";

//==============================================================
// Display pin assignments
//==============================================================

#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  3
#define TFT_MOSI 10
#define TFT_SCLK 8

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

//==============================================================
// Gauge settings
//==============================================================

const int cx = 120;          // Center X of the gauge
const int cy = 120;          // Center Y of the gauge
const int radius = 120;      // Outer radius of the gauge
const int needleLength = 98; // Length of the needle
const int sensorPin = 2;     // ADC pin for pressure sensor

// Last gauge value used for erasing/redrawing the needle.
// This should stay consistent with whatever unit the needle uses.
float oldAngle = 225;

//==============================================================
// Draw the gauge tick marks
//==============================================================

void drawTicks() {
  for (int psi = 0; psi <= 100; psi += 5) {
    // Map 0-100 PSI onto the 270-degree gauge sweep.
    float angleDeg = 45.0 - (psi * 270.0 / 100.0);
    float angleRad = angleDeg * DEG_TO_RAD;

    int x1, y1, x2, y2;

    // Major tick marks at 0, 25, 50, 75, 100.
    if (psi % 25 == 0) {
      x1 = cx + cos(angleRad) * (radius - 20);
      y1 = cy + sin(angleRad) * (radius - 20);
      x2 = cx + cos(angleRad) * radius;
      y2 = cy + sin(angleRad) * radius;

      tft.drawLine(x1, y1, x2, y2, GC9A01A_WHITE);
      tft.drawLine(x1 + 1, y1, x2 + 1, y2, GC9A01A_WHITE);
      tft.drawLine(x1 - 1, y1, x2 - 1, y2, GC9A01A_WHITE);
    } else {
      // Minor tick marks every 5 PSI.
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

//==============================================================
// Draw the numeric labels on the gauge
//==============================================================

void drawLabels() {
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);

  tft.setCursor(50, 175);
  tft.print("0");

  tft.setCursor(34, 80);
  tft.print("25");

  tft.setCursor(109, 28);
  tft.print("50");

  tft.setCursor(182, 80);
  tft.print("75");

  tft.setCursor(165, 175);
  tft.print("100");
}

//==============================================================
// Draw the colored arc around the gauge
//==============================================================

void drawColorArc() {
  for (int psi = 0; psi <= 100; psi++) {
    uint16_t color;

    // Green = normal range, orange = warning, red = high.
    if (psi < 80)
      color = GC9A01A_GREEN;
    else if (psi < 90)
      color = GC9A01A_ORANGE;
    else
      color = GC9A01A_RED;

    float angleDeg = 135.0 + (psi * 270.0 / 100.0);
    float angleRad = angleDeg * DEG_TO_RAD;

    int x = cx + cos(angleRad) * (radius - 3);
    int y = cy + sin(angleRad) * (radius - 3);

    tft.fillCircle(x, y, 2, color);
  }
}

//==============================================================
// Draw the needle at the requested pressure value
//==============================================================

void drawNeedle(float psi, uint16_t color) {
  float angleDeg = 135.0 + (psi * 270.0 / 100.0);
  float angleRad = angleDeg * DEG_TO_RAD;

  int x = cx + cos(angleRad) * needleLength;
  int y = cy + sin(angleRad) * needleLength;

  tft.drawLine(cx, cy, x, y, color);

  // Thicken the needle for better visibility.
  tft.drawLine(cx + 1, cy, x + 1, y, color);
  tft.drawLine(cx - 1, cy, x - 1, y, color);

  // Draw the center hub.
  tft.fillCircle(cx, cy, 4, GC9A01A_WHITE);
}

//==============================================================
// Draw the digital pressure readout
//==============================================================

void drawPressureValue(float psi) {
  // Clear the old text first so the number does not smear.
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

//==============================================================
// MQTT reconnect helper
//==============================================================

void reconnect() {
  if (client.connected()) {
    return;
  }

  Serial.print("Attempting MQTT connection...");

  if (client.connect("ESP32Gauge", "myuser", "2N3904")) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

//==============================================================
// Setup
//==============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi.
  WiFi.begin(ssid, password);
  Serial.println("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected!");
  Serial.println(WiFi.localIP());

  // Configure MQTT broker.
  client.setServer("192.168.1.218", 1883);

  //==========================================================
  // OTA setup
  //==========================================================

  ArduinoOTA.setHostname("IrrigationGauge");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Started");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]\n", error);
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");

  //==========================================================
  // Initialize display
  //==========================================================

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setDataMode(SPI_MODE3);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  drawColorArc();
  drawTicks();
  drawLabels();

  Serial.println("Startup Complete");
}

//==============================================================
// Main loop
//==============================================================

void loop() {
  ArduinoOTA.handle();

  // Keep MQTT alive and reconnect if needed.
  static unsigned long lastReconnect = 0;

  if (!client.connected()) {
    if (millis() - lastReconnect >= 5000) {
      lastReconnect = millis();
      reconnect();
    }
  } else {
    client.loop();
  }

  // Read and average multiple ADC samples to reduce noise.
  long total = 0;
  for (int i = 0; i < 64; i++) {
    total += analogRead(sensorPin);
    delay(2);
  }
  int adc = total / 64;

  // Convert ADC value into PSI using your calibration points.
  float psi = (adc - 680) * 100.0 / (3260 - 680);

  Serial.println(adc);

  // Publish pressure value to MQTT.
  char msg[10];
  dtostrf(psi, 1, 1, msg);
  client.publish("shop/pressure", msg);

  // Redraw the needle and value.
  drawNeedle(oldAngle, GC9A01A_BLACK);
  drawLabels();
  tft.fillCircle(cx, cy, 6, GC9A01A_WHITE);
  drawNeedle(psi, GC9A01A_RED);
  drawPressureValue(psi);

  oldAngle = psi;
  delay(100);
}