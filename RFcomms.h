#include <Arduino.h>
/*
ESP Modification: John Archbold

Sketch Date: August 16th, 2017
Sketch Version: V3.2.1

Implements mDNS discovery of MQTT broker
Implements definitions for
  ESP-NodeMCU
  ESP8266
  WROOM32
Communications Protocol
  WiFi
Communications Method
  MQTT        Listens for messages on Port 1883
*/

#ifndef RFWiFi_H
#define RFWiFi_H

// ==============================================================
// Make sure to update this for your own WiFi and/or MQTT Broker!
// ==============================================================

#define MQTT_broker_default  "mqttbroker"    // Default hostname of mqtt broker
#define MQTT_broker_username "mqttuser"      // Required if broker security is enabled
#define MQTT_broker_password "mqttpass"
#define MQTT_port 1883

#define UDP_port 2390
#define NTP_port 123

#define RFLink_SSID "HAPInet"
#define RFLink_PWD  "HAPIconnect"


#endif //RFWiFi_H
