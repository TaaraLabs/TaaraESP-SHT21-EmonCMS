/* 
 * Send temperature and humidity data from SHT21/HTU21 sensor on ESP8266 to EmonCMS over HTTPS or HTTP
 * 
 * Demo for TaaraESP SHT21 Wifi Humidity sensor https://taaralabs.eu/es1
 * 
 * by Märt Maiste, TaaraLabs
 * https://github.com/TaaraLabs/TaaraESP-SHT21-EmonCMS
 * 2016-08-08
 *
 * Configuration
 * First, the EmonCMS account is needed either on https://emoncms.org or on a private EmonCMS server.
 * For entering the WiFi and EmonCMS settings go to config mode - press config button in one second AFTER powering on.
 * The status LED should stay on.
 * On your WiFi enabled mobile device or laptop search for WiFi AP-s with the name starting with ESP and following few numbers (module ID) - connect to it.
 * In most cases the device is automatically redirected to the configuration web page on http://192.168.4.1
 * Choose the WiFi network from the list or enter it manually, enter WiFi password, EmonCMS server name and APIKEY and save settings.
 * The board should restart and enter the normal operational mode.
 * 
 * Normal operation
 * At the startup the status LED goes on and stays on for few seconds until the WiFi connection is established.
 * If connecting to the server and sending the data request is successful then the LED stays off and the module enters deep sleep for five minutes.
 * After deep sleep the module reboots and everything starts from the beginning.
 * In short - in normal operation the status LED should go on for few seconds after every five minutes.
 * 
 * Diagnostics
 * LED stays on longer than 5 seconds - problem with WiFi connection or the module is in config mode. WiFi is not configured properly or not in range.
 * LED blinks once at a time - could not connect to server. Server is not configured or not accessible.
 * LED blinks twice at a time - problem with the request. Server did not answer with "ok". Wrong apikey, error in data format or server/app is broken.
*/

#include <ESP8266WiFi.h>       // https://github.com/esp8266/Arduino
#include <WiFiClientSecure.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager

#include "SparkFunHTU21D.h"    // https://github.com/sparkfun/SparkFun_HTU21D_Breakout_Arduino_Library

#include <EEPROM.h>

#define DEBUG 0          // serial port debugging
#define HTTPS 1          // use secure connection

const int interval  = 5; // send readings every 5 minutes
const int LED       = 13;
const int BUTTON    = 0; // program button is available after power-up for entering configPortal

struct CONFIG {          // config data is very easy to read and write in the EEPROM using struct
  char host[40];         // EmonCMS server name
  char apikey[33];       // EmonCMS APIKEY
};

CONFIG conf;             // initialize conf struct

bool save = false;       // WifiManager will not change the config data directly
void saveConfig () {     // it uses callback function instead
  save = true;           // mark down that something has been changed via WifiManager
}

// blink the LED shortly for number of times, wait a second and repeat for 5 minutes
void blink (byte times) {
  unsigned long start = millis();
  while (millis() - start < 5 * 60 * 1000) {
    for (byte i=0; i < times; i++) {
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
    }
    delay(1000);
  }  
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println("\n\nTaaraESP SHT21 [taaralabs.eu/es1]\n");
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH); // turn the led on before taking new measurement and connecting to wifi AP

  HTU21D sht;
  Wire.begin(2,14);        // overriding the i2c pins
  //sht.begin();           // default: sda=4, scl=5
  float humidity    = sht.readHumidity();
  float temperature = sht.readTemperature();

  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);

  #ifdef DEBUG
  Serial.print("ESP chip ID: ");
  Serial.println(ESP.getChipId());
  #ifdef HTTPS
  Serial.println("Using secure connection");
  #endif
  #endif

  EEPROM.begin(100); // using 100 bytes of flash for config data
  EEPROM.get<CONFIG>(0, conf);
  #ifdef DEBUG
  Serial.print("host: ");   Serial.println(conf.host);
  Serial.print("apikey: "); Serial.println(conf.apikey);
  #endif

  WiFiManagerParameter custom_host("conf.host", "Emoncms server", conf.host, 40);
  WiFiManagerParameter custom_apikey("conf.apikey", "API key", conf.apikey, 33);
  
  WiFiManager wifiManager;
  #ifndef DEBUG
  wifiManager.setDebugOutput(false);
  #endif
  wifiManager.setTimeout(300); // if wifi is not configured or not accessible then wait 5 minutes and then move on
  wifiManager.setSaveConfigCallback(saveConfig);
  wifiManager.addParameter(&custom_host);
  wifiManager.addParameter(&custom_apikey);

  pinMode(BUTTON, INPUT);

  #ifdef DEBUG
  Serial.println("Wait a sec for config button");
  #endif
  unsigned long start = millis();

  while (millis() - start < 1000) {                    // check if config button was pressed in one second after power up
    if (digitalRead(BUTTON) == LOW) {
      Serial.println("Config button was pressed");
      wifiManager.setTimeout(0);                       // no timeout if the button was pressed
      //String ssid = "ESP" + String(ESP.getChipId());
      //wifiManager.startConfigPortal(ssid.c_str());
      wifiManager.startConfigPortal();                 // start the config portal
      break;
      }
    delay(100);
    }
  #ifdef DEBUG
  Serial.println("Config button was not pressed");
  #endif

  if (save == true) {                                       // if save flag is set by the callback function of the wifiManager
    Serial.println("Saving config");
    strcpy(conf.host,   custom_host.getValue());            // read the data from wifiManager registers
    strcpy(conf.apikey, custom_apikey.getValue());
    #ifdef DEBUG
    Serial.print("host: ");   Serial.println(conf.host);
    Serial.print("apikey: "); Serial.println(conf.apikey);
    #endif
    EEPROM.put<CONFIG>(0, conf);                            // write the config data to the flash
    EEPROM.commit();
    delay(10);
  }
  
  if (wifiManager.autoConnect()) { // try to connect to wifi AP using stored credentials
  
    digitalWrite(LED, LOW);        // if wifi connection is established then the led goes out (or stays on if there is a problem with wifi connection)

    #ifdef HTTPS
    WiFiClientSecure client;
    const int port = 443;
    #else
    WiFiClient client;
    const int port = 80;
    #endif
    
    #ifdef DEBUG  
    Serial.print("Connecting to ");
    Serial.print(conf.host);
    Serial.print(":");
    Serial.println(port);
    #endif
    
    if (!client.connect(conf.host, port)) {
      #ifdef DEBUG
      Serial.println("Connection failed");
      #endif
      blink(1);    // blink 1 time, wait a second and repeat for 5 minutes
      ESP.reset(); // reset the board
      delay(5000); // resetting may take some time
    }

    uint8_t m[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(m);
    char mac[20];
    sprintf(mac,"%02X%02X%02X%02X%02X%02X",m[0],m[1],m[2],m[3],m[4],m[5]);

    String url = "/input/post.json?node=1&apikey="; // node=1 is needed for older EmonCMS installations
    url += conf.apikey;
    url += "&json={";
    url += mac;                                     // the WiFi MAC address is used as sensor ID in the EmonCMS
    url += ".T:";                                   // .T is added for temperature
    url += temperature;
    url += ",";
    url += mac;
    url += ".H:";                                   // .H for humidity
    url += humidity;
    url += "}";

    #ifdef DEBUG
    Serial.print("Requesting URL: ");
    Serial.println(url);
    #endif    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + conf.host + "\r\n" + 
                 "Connection: close\r\n\r\n");
    delay(100);                                     // let the server to process the request, also the module needs some time off for background tasks

    bool ok = false;
    while(client.available()) {
      String line = client.readStringUntil('\n');   // read until the end of header and body 
      #ifdef DEBUG
      Serial.println(line);
      #endif
      if (line.startsWith("ok")) {                  // EmonCMS confirms that the request was "ok"
        ok = true;
        Serial.println("Server accepted the data");
      }
    }
    
    if (ok == false) { // if "ok" was not received because apikey was wrong, error in data format or the server/app is broken (at the moment)
      blink(2);        // blink 2 times, wait a second and repeat for 5 minutes
      ESP.reset();
      delay(5000);
    }
    
    #ifdef DEBUG
    Serial.println();
    Serial.println("Closing connection");
    #endif
  }
  
  Serial.print("Going to sleep for ");
  Serial.print(interval);
  Serial.println(" minutes");
  delay(3000);                            // let the module to get everything in order in the background
  ESP.deepSleep(interval * 60 * 1000000); // deep sleep means turning off everything except the wakeup timer. wakeup call resets the chip
  delay(5000);                            // going to sleep may take some time
}

void loop() {
  // we should never reach this point, but if we do, then blink rapidly
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
  delay(100);
}
