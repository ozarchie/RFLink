#include <Arduino.h>

// RFLink
// ======
#include "RFLink.h"       // RFLink configurations
#include "Config_01.c"    //  and the protocol configurations

// ESP32 WiFi
// ==========
#include <WiFi.h>
#include <ESPmDNS.h>      // for avahi
#include <WiFiUdp.h>      // For ntp
#include "RFcomms.h"      // RFLink WiFi and MQTT configurations

// ESP32 MQTT
// ==========
#include <PubSubClient.h> // Allows us to connect to, and publish to the MQTT brokerinclude <stdlib.h>
#include <ArduinoJson.h>  // JSON library for MQTT strings
#include <math.h>

// Real Time Clock Libraries
// Time related libararies
#include <DS1307RTC.h>     //https://www.pjrc.com/teensy/td_libs_DS1307RTC.html
#include <TimeLord.h>      //https://github.com/probonopd/TimeLord
#include <TimeLib.h>       //https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>    //https://github.com/PaulStoffregen/TimeAlarms
//#include <Timezone.h>      //https://github.com/schizobovine/Timezone (https://github.com/JChristensen/Timezone)

#include <Wire.h>
#include <Bounce2.h>        // Used for "debouncing" inputs


// RFLink globals
// ==============
byte PKSequenceNumber=0;                                                        // 1 byte packet counter
boolean RFDebug=false;                                                          // debug RF signals with plugin 001 
boolean RFUDebug=false;                                                         // debug RF signals with plugin 254 
boolean QRFDebug=false;                                                         // debug RF signals with plugin 254 but no multiplication

char pbuffer[PRINT_BUFFER_SIZE];                                                // Buffer for printing data
char InputBuffer_Serial[INPUT_COMMAND_SIZE];                                    // Buffer for Serial data

struct RawSignalStruct                                                          // Raw signal variables placed in a struct
  {
  int  Number;                                                                  // Number of pulses, times two as every pulse has a mark and a space.
  byte Repeats;                                                                 // Number of re-transmits on transmit actions.
  byte Delay;                                                                   // Delay in ms. after transmit of a single RF pulse packet
  byte Multiply;                                                                // Pulses[] * Multiply is the real pulse time in microseconds 
  unsigned long Time;                                                           // Timestamp indicating when the signal was received (millis())
  unsigned int Pulses[RAW_BUFFER_SIZE + 2];                                               // Table with the measured pulses in microseconds divided by RawSignal.Multiply. (halves RAM usage)
                                                                                // First pulse is located in element 1. Element 0 is used for special purposes, like signalling the use of a specific plugin
} RawSignal={0,0,0,0,0,0L};

// RFRaw Globals
// =============

const unsigned long LoopsPerMilli = 15000;       // ESP32 does 15k loops in 1 mS!
const unsigned long Overhead = 0;  

int RawCodeLength=0;
unsigned long PulseLength = 0L;
unsigned long numloops = 0L;
unsigned long maxloops = 0L;

unsigned int startuS;
unsigned int finishuS;

boolean Ftoggle=false;
uint8_t Fbit=0;
uint8_t Fport=0;
uint8_t FstateMask=0;

uint8_t firsttime = 0;

// Plugin globals
// ==============
unsigned long RepeatingTimer=0L;
unsigned long SignalCRC=0L;            // holds the bitstream value for some plugins to identify RF repeats
unsigned long SignalHash=0L;           // holds the processed plugin number
unsigned long SignalHashPrevious=0L;   // holds the last processed plugin number

// WiFi configuration
// ==================
// the media access control address for the ESP32
byte mac[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
char mac_str[16] = "555555555555";  // Default mac id      
char hostString[64] = {0};          // mDNS Hostname for this RFLink

const char* ssid = RFLink_SSID;
const char* password = RFLink_PWD;
int WifiStatus = 0;
WiFiClient RFClient;
WiFiUDP udp;                                          // UDP instance to send and receive packets over UDP

// ntp config
// ==========
IPAddress ntpServerIP;                                // Place to store IP address of mqttbroker.local
char ntpServer_hostname[64] = MQTT_broker_default;    // Assume mqttbroker is also the time server
const int NTP_PACKET_SIZE = 48;                       // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];                  //  buffer to hold incoming and outgoing packets
const unsigned int localPort = UDP_port;              // local port to listen for UDP packets

unsigned long mscount;      // millisecond counter
time_t epoch;               // UTC seconds
time_t currentTime;         // Local value

int timeZone = +10; // Eastern Standard Time (Au)
//int timeZone = -5;  // Eastern Standard Time (USA)
//int timeZone = -4;  // Eastern Daylight Time (USA)
//int timeZone = -8;  // Pacific Standard Time (USA)
//int timeZone = -7;  // Pacific Daylight Time (USA)


// MQTT Configuration
// ==================
char MQTT_broker_hostname[64] = MQTT_broker_default;    // Space to hold mqtt broker hostname
const char* clientID = "RFLink";
const char* mqtt_topic_command = "COMMAND/";            // General Command topic
const char* mqtt_topic_status = "STATUS/RESPONSE/";     // General Status topic
const char* mqtt_topic_asset = "ASSET/RESPONSE/";       // Genral Asset topic
const char* mqtt_topic_exception = "EXCEPTION/";        // General Exception topic
const char* mqtt_topic_config = "CONFIG/";              // General Configuration topic
char mqtt_topic[256] = "";                              // Topic for this RFLink device

#define MAXTOPICS 5
#define MAXLISTEN 12
#define STATUSSTART 0
#define ASSETSTART 1
#define CONFIGSTART 4

const char* mqtt_topic_array[MAXTOPICS] = {
  "STATUS/QUERY",
  "ASSET/QUERY",
  "ASSET/QUERY/",
  "ASSET/QUERY/*",
  "CONFIG/QUERY/"
};

const char* mqtt_listen_array[MAXLISTEN] = {
  "COMMAND/",
  "CONFIG/",
  "EXCEPTION/",
  "STATUS/QUERY",
  "STATUS/QUERY/",
  "STATUS/QUERY/#",
  "ASSET/QUERY",
  "ASSET/QUERY/",
  "ASSET/QUERY/#",
  "CONFIG/QUERY",
  "CONFIG/QUERY/",
  "CONFIG/QUERY/#"
};

char MQTTOutput[512];                                   // String storage for the JSON data
char MQTTInput[512];                                    // String storage for the JSON data

// Callback function header
void MQTTcallback(char* topic, byte* payload, unsigned int length);
PubSubClient MQTTClient(RFClient);                      // 1883 is the listener port for the Broker

// Prepare JSON
// ============
StaticJsonBuffer<512> RF_topic_exception;
JsonObject& exception_topic = RF_topic_exception.createObject();



void setup() {
  Serial.begin(BAUD);                                                           // Initialise the serial port
  while (!Serial) ;

  pinMode(PIN_RF_RX_DATA, INPUT);                                               // Initialise in/output ports
  pinMode(PIN_RF_TX_DATA, OUTPUT);                                              // Initialise in/output ports
  pinMode(PIN_RF_TX_VCC,  OUTPUT);                                              // Initialise in/output ports
  pinMode(PIN_RF_RX_VCC,  OUTPUT);                                              // Initialise in/output ports    
  digitalWrite(PIN_RF_RX_VCC,HIGH);                                             // turn VCC to RF receiver ON
  digitalWrite(PIN_RF_RX_DATA,INPUT_PULLUP);                                    // pull-up resister on (to prevent garbage)  
  pinMode(PIN_BSF_0,OUTPUT);                                                    // rflink board switch signal
  digitalWrite(PIN_BSF_0,HIGH);                                                 // rflink board switch signal
  
// Main Code Setup
// ===============

  WifiStartup();                                                                   // Initialize Wifi, NTP 

  mscount = millis();   // initialize the millisecond counter

// Start MQTT support
// ==================
  if (WiFi.status() == WL_CONNECTED)  MQTTStartup();
  
  currentTime = now();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("MQTT Setup Complete. Listening for topics .."));
    }
    else {
      Serial.println(F("WiFi not connected, trying again later .."));     
    }
  
  Serial.println(""); 
  Serial.print(F("20;00;Nodo RadioFrequencyLink - RFLink Gateway V1.1 - "));
  sprintf(InputBuffer_Serial,"R%02x;",REVNR);
  Serial.println(InputBuffer_Serial); 

  PKSequenceNumber++;
  PluginInit();
  PluginTXInit();
}

void loop() {
//  byte SerialInByte=0;                                                          // incoming character value
//  int SerialInByteCounter=0;                                                    // number of bytes counter 

//  byte ValidCommand=0;
//  unsigned long FocusTimer=0L;                                                  // Timer to keep focus on the task during communication
//  InputBuffer_Serial[0]=0;                                                      // erase serial buffer string 

  while(true) {

    if (WiFi.status() != WL_CONNECTED)  {
      WifiRetryConnect();
//      if (WiFi.status() == WL_CONNECTED)  WifiRetryMDNS();
      if (WiFi.status() == WL_CONNECTED)  MQTTStartup();
    }
    RawSignal.Time = millis();                                                  // Time the RF packet was received (to keep track of retransmits)
    ScanEvent();                                                                // Scan for RF events

    MQTTClient.loop();                                                          // Check for MQTT topics
    Alarm.delay(0);      
/*
    if(Serial.available()) {
      FocusTimer=millis()+FOCUS_TIME;

      while(FocusTimer>millis()) {                                              // standby 
        if(Serial.available()) {
          SerialInByte=Serial.read();                
          
          if(isprint(SerialInByte))
            if(SerialInByteCounter<(INPUT_COMMAND_SIZE-1))
              InputBuffer_Serial[SerialInByteCounter++]=SerialInByte;
              
          if(SerialInByte=='\n') {                                              // new line character
            InputBuffer_Serial[SerialInByteCounter]=0;                          // serieel data is complete
            //Serial.print("20;incoming;"); 
            //Serial.println(InputBuffer_Serial); 
            if (strlen(InputBuffer_Serial) > 7){                                // need to see minimal 8 characters on the serial port
               // 10;....;..;ON; 
               if (strncmp (InputBuffer_Serial,"10;",3) == 0) {                 // Command from Master to RFLink
                  // -------------------------------------------------------
                  // Handle Device Management Commands
                  // -------------------------------------------------------
                  if (strcasecmp(InputBuffer_Serial+3,"PING;")==0) {
                     sprintf(InputBuffer_Serial,"20;%02X;PONG;",PKSequenceNumber++);
                     Serial.println(InputBuffer_Serial); 
                  } else
                  if (strcasecmp(InputBuffer_Serial+3,"REBOOT;")==0) {
                     strcpy(InputBuffer_Serial,"reboot");
                     Reboot();
                  } else
                  if (strncasecmp(InputBuffer_Serial+3,"RFDEBUG=O",9) == 0) {
                     if (InputBuffer_Serial[12] == 'N' || InputBuffer_Serial[12] == 'n' ) {
                        RFDebug=true;                                           // full debug on
                        RFUDebug=false;                                         // undecoded debug off 
                        QRFDebug=false;                                        // undecoded debug off
                        sprintf(InputBuffer_Serial,"20;%02X;RFDEBUG=ON;",PKSequenceNumber++);
                     } else {
                        RFDebug=false;                                          // full debug off
                        sprintf(InputBuffer_Serial,"20;%02X;RFDEBUG=OFF;",PKSequenceNumber++);
                     }
                     Serial.println(InputBuffer_Serial); 
                  } else                 
                  if (strncasecmp(InputBuffer_Serial+3,"RFUDEBUG=O",10) == 0) {
                     if (InputBuffer_Serial[13] == 'N' || InputBuffer_Serial[13] == 'n') {
                        RFUDebug=true;                                          // undecoded debug on 
                        QRFDebug=false;                                        // undecoded debug off
                        RFDebug=false;                                          // full debug off
                        sprintf(InputBuffer_Serial,"20;%02X;RFUDEBUG=ON;",PKSequenceNumber++);
                     } else {
                        RFUDebug=false;                                         // undecoded debug off
                        sprintf(InputBuffer_Serial,"20;%02X;RFUDEBUG=OFF;",PKSequenceNumber++);
                     }
                     Serial.println(InputBuffer_Serial); 
                  } else                 
                  if (strncasecmp(InputBuffer_Serial+3,"QRFDEBUG=O",10) == 0) {
                     if (InputBuffer_Serial[13] == 'N' || InputBuffer_Serial[13] == 'n') {
                        QRFDebug=true;                                         // undecoded debug on 
                        RFUDebug=false;                                         // undecoded debug off 
                        RFDebug=false;                                          // full debug off
                        sprintf(InputBuffer_Serial,"20;%02X;QRFDEBUG=ON;",PKSequenceNumber++);
                     } else {
                        QRFDebug=false;                                        // undecoded debug off
                        sprintf(InputBuffer_Serial,"20;%02X;QRFDEBUG=OFF;",PKSequenceNumber++);
                     }
                     Serial.println(InputBuffer_Serial); 
                  } else                 
                  if (strncasecmp(InputBuffer_Serial+3,"VERSION",7) == 0) {
                      sprintf(InputBuffer_Serial,"20;%02X;VER=1.1;REV=%02x;BUILD=%02x;",PKSequenceNumber++,REVNR, BUILDNR);
                      Serial.println(InputBuffer_Serial); 
                  } else {
                     // -------------------------------------------------------
                     // Handle Generic Commands / Translate protocol data into Nodo text commands 
                     // -------------------------------------------------------
                     // check plugins
                     if (InputBuffer_Serial[SerialInByteCounter-1]==';') InputBuffer_Serial[SerialInByteCounter-1]=0;  // remove last ";" char
                     if(PluginTXCall(0, InputBuffer_Serial)) {
                        ValidCommand=1;
                     } else {
                        // Answer that an invalid command was received?
                        ValidCommand=2;
                     }
                  }
               }
            } // if > 7
            if (ValidCommand != 0) {
               if (ValidCommand == 1) {
                  sprintf(InputBuffer_Serial,"20;%02X;OK;",PKSequenceNumber++);
                  Serial.println( InputBuffer_Serial ); 
               } else {
                  sprintf(InputBuffer_Serial, "20;%02X;CMD UNKNOWN;", PKSequenceNumber++); // Node and packet number 
                  Serial.println( InputBuffer_Serial );
               }   
            }
            SerialInByteCounter = 0;  
            InputBuffer_Serial[0] = 0;                                            // serial data has been processed. 
            ValidCommand = 0;
            FocusTimer = millis() + FOCUS_TIME;                                             
          }// if(SerialInByte
       }// if(Serial.available())
    }// while 
   }// if(Serial.available())
*/
  }// while 
}




