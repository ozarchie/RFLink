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

