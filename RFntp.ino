/*
ESP Modification: John Archbold

Sketch Date: August 16th, 2017
Sketch Version: V3.2.1
Implement of MQTT-based HAPInode (HN) for use in Monitoring and Control
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

time_t getNtpTime(void) {
  if (WiFi.status() == WL_CONNECTED) {
    while (udp.parsePacket() > 0) ; // discard any previously received packets
    Serial.println(F("Transmit NTP Request"));
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Serial.println(F("Receive NTP Response"));
        udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
  
    // The timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    Serial.println(F("No NTP Response :-("));
    return 0; // return 0 if unable to get the time
  }
  return 0; // No WiFi
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  Serial.println(F("sending NTP packet..."));
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, NTP_port);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void setupTime(void) {
  setSyncInterval(60);        // Set minimum seconds between re-sync via now() call
  setSyncProvider(RTC.get);   // Get the time from the RTC during operation
  epoch = getNtpTime();       // Get the time from ntp (if available)
  if (epoch != 0) {
    setTime(epoch);           // getNtpTime() returns 0 if no ntp server
    Serial.println(F("ntp has set the system time"));
  }
  if(timeStatus() != timeSet) {   // Has the RTC been set?
     Serial.println(F("RTC not set and no ntp server !!"));
     return;
  }
  Serial.println(F("RTC has the system time"));
}

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

  Serial.println(WiFi.getHostname());
  Serial.println(WiFi.setHostname("RFLink"));
  Serial.print(F("NewHostname: "));
  Serial.println(WiFi.getHostname());
  Serial.print(F("IP  address: "));
  Serial.println(WiFi.localIP());

// Start mDNS support
// ==================
  strcpy(hostString, WiFi.getHostname()); // Get current hostname
  strcat(hostString, mac_str);            // Add 3 bytes of mac ID
  Serial.print(F("hostString: "));
  Serial.println(hostString);

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

// Start NTP support
// =================
  Serial.println(F("Starting UDP"));                 // Start UDP
  udp.begin(localPort);
  Serial.print(F("Local port: "));
  WiFi.hostByName(ntpServer_hostname, ntpServerIP);   // Get mqttbroker's IP address
  Serial.print(F("Local IP:   "));
  Serial.println(ntpServerIP);

  setupTime();          // initialize RTC using ntp, if available
  
}
