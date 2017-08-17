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

void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        break;
    }
}

void WifiStartup(void) {
    WifiRetryConnect();
    if (WiFi.status() == WL_CONNECTED)  WifiRetryMDNS();
}

void WifiRetryConnect(void) {
  
  // delete old config
  WiFi.disconnect(true);
  delay(1000);

  WiFi.onEvent(WiFiEvent);
  
  Serial.print(F("Initializing WiFi network ... "));
  Serial.print(ssid);
  Serial.print(F(" - "));
  Serial.println(password);
  
  WifiStatus = WiFi.begin(ssid, password);
  delay(1000);
  
  while ( (WiFi.status() != WL_CONNECTED) && (numloops++ < 32) ) {
    delay(500);
    Serial.print(F("."));
  }
  WiFi.onEvent(WiFiEvent);
}


void WifiRetryMDNS(void) {

  WiFi.macAddress(mac);
  b2c(&mac[3], &mac_str[0], 3);         //convert mac to ASCII value for unique station ID

  Serial.print(F("ChipHN: "));        // print reset hostname
  Serial.println(WiFi.getHostname());

  strcpy(hostString, "RFLink");           // Set hostname base
  strcat(hostString, mac_str);            // Add 3 bytes of mac ID
  Serial.println(WiFi.setHostname(hostString));
  Serial.print(F("Hostname:    "));
  Serial.println(WiFi.getHostname());
  Serial.print(F("IP  address: "));
  Serial.println(WiFi.localIP());
    
// Start mDNS support
// ==================

 if (!MDNS.begin(hostString)) {
    Serial.println(F("Error setting up MDNS responder!"));
  }
  Serial.print(F("Hostname: "));
  Serial.print(hostString);
  Serial.println(F(" mDNS responder started for RFLink"));

  Serial.print(F("Sending mDNS query to find mqtt broker - "));
  int n = 0;
  numloops = 0;
  while ( (n == 0) && (numloops++ < 5) ) {
    n = MDNS.queryService("mqtt", "tcp"); // Send out query for workstation tcp services
    Serial.println(F("mDNS query done"));
    if (n == 0) {
      Serial.println(F("no services found"));
    }
    else {
      Serial.print(n);
      Serial.println(F(" service(s) found"));
      for (int i = 0; i < n; ++i) {
        // Print details for each service found
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.print(MDNS.hostname(i));
        Serial.print(F(" ("));
        Serial.print(MDNS.IP(i));
        Serial.print(F(":"));
        Serial.print(MDNS.port(i));
        Serial.println(F(")"));
        if (MDNS.port(i) == MQTT_port) {
          MDNS.hostname(i).toCharArray(MQTT_broker_hostname,(MDNS.hostname(i).length()+1));
  // TODO check for separate ntp server
          MDNS.hostname(i).toCharArray(ntpServer_hostname,(MDNS.hostname(i).length()+1));
        }
      }
    }
  }
  
  Serial.print(F("Hostname: "));
  Serial.print(MQTT_broker_hostname);
  Serial.println(F(" being used for MQTT_broker_hostname"));
  Serial.print(F("Hostname: "));
  Serial.print(ntpServer_hostname);
  Serial.println(F(" being used for ntpServer"));
  Serial.println();

  setupTime();
}
