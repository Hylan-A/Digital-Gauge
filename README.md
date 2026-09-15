# gauge_finishdesign1_OTA
This sketch is an ESP32-based digital pressure gauge with OTA update support, Wi-Fi connectivity, MQTT publishing, and a round GC9A01A TFT gauge display. It reads an analog pressure sensor on sensorPin, averages 64 ADC samples for smoother readings, converts the ADC value into PSI using calibration points, and displays both a graphical needle gauge and a numeric PSI readout. The display includes a colored arc, tick marks, and labeled scale points from 0 to 100 PSI. The code also keeps the device available for Arduino OTA firmware updates and publishes the measured pressure to an MQTT topic (shop/pressure) for remote monitoring.

A few implementation details stand out: the gauge is drawn with helper functions for ticks, labels, color banding, needle rendering, and the pressure text; MQTT reconnection is handled in the main loop; and the code continuously calls ArduinoOTA.handle() so updates can be accepted at any time. The sketch is well organized and already close to a finished product.

One important note: the Wi-Fi SSID/password and MQTT credentials are hard-coded in the sketch, so those should be moved out of the public repo or replaced before sharing.
