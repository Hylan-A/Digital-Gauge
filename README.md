# gauge_finishdesign1_OTA
This sketch is an ESP32-based digital pressure gauge with OTA update support, Wi-Fi connectivity, MQTT publishing, and a round GC9A01A TFT gauge display. It reads an analog pressure sensor on sensorPin, averages 64 ADC samples for smoother readings, converts the ADC value into PSI using calibration points, and displays both a graphical needle gauge and a numeric PSI readout. The display includes a colored arc, tick marks, and labeled scale points from 0 to 100 PSI. The code also keeps the device available for Arduino OTA firmware updates and publishes the measured pressure to an MQTT topic (shop/pressure) for remote monitoring.

A few implementation details stand out: the gauge is drawn with helper functions for ticks, labels, color banding, needle rendering, and the pressure text; MQTT reconnection is handled in the main loop; and the code continuously calls ArduinoOTA.handle() so updates can be accepted at any time. The sketch is well organized and already close to a finished product.
Here’s a clean, accurate summary of the main points from the page you’re viewing, grounded directly in the page content.

---

### 🔧 Project Overview  
- The repository contains an **ESP32‑based digital pressure gauge** with **OTA updates**, **Wi‑Fi**, **MQTT**, and a **GC9A01A round TFT display**   [Current page](citation-section://1374943435/2).

### 📡 Pressure Measurement  
- Reads an **analog pressure sensor** on `sensorPin`.  
- Averages **64 ADC samples** for smoother readings.  
- Converts ADC values into **PSI** using calibration points   [Current page](citation-section://1374943435/2).

### 📊 Display Features  
- Shows a **graphical needle**, **numeric PSI**, **colored arc**, **tick marks**, and **labels** from **0–100 PSI**   [Current page](citation-section://1374943435/3).

### 🌐 Connectivity & Remote Monitoring  
- Device stays available for **Arduino OTA firmware updates**.  
- Publishes pressure readings to MQTT topic **shop/pressure** for remote monitoring   [Current page](citation-section://1374943435/4).

### 🛠️ Code Structure  
- Gauge drawn using helper functions for ticks, labels, color bands, needle rendering, and pressure text.  
- MQTT reconnection handled in the main loop.  
- `ArduinoOTA.handle()` runs continuously to accept updates anytime   [Current page](citation-section://1374943435/5).

### ⚠️ Security Note  
- **Wi‑Fi SSID/password and MQTT credentials are hard‑coded** and should be removed or replaced before sharing publicly   [Current page](citation-section://1374943435/7).

### 📦 Repository Details  
- No releases or packages published.  
- Single contributor.  
- Codebase is **100% C++**   [Current page](citation-section://1374943435/8).

---

If you want, I can also break down the architecture, suggest improvements, or help you extend the gauge’s features.

One important note: the Wi-Fi SSID/password and MQTT credentials are hard-coded in the sketch, so those should be moved out of the public repo or replaced before sharing.
