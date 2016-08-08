# TaaraESP-SHT21-EmonCMS

[TaaraESP-SHT21 WebSite](https://taaralabs.eu/es1)

Arduino sketch for sending TaaraESP-SHT21 (ESP8266+SHT21) temperature and humidity sensor data to EmonCMS over HTTPS or HTTP

The basic idea of this program is to start the board up, take the measurements as soon as possible before the board gets a chance to heat up, then establish The Wifi connection, connect to the EmonCMS server and send the data over HTTPS or HTTP as in JSON format. After that the board goes into deep sleep (shuts down) for five minutes and only the timer keeps going. In this mode the board consumes very little power and generates almost no heat which could affect the accuracy of the temperature and humidity measurements. Waking up from deep sleep reboots the board and the program starts from the beginning.

The on-board LED on the TaaraESP-SHT21 is used for diagnostics indicating the state of the device.
