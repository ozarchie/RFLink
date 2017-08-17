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

#ifndef RF433_H
#define RF433_H

#define BUILDNR                         0x07                                    // shown in version
#define REVNR                           0x33                                    // shown in version and startup string
#define RFLINK_FW_VERSION               (BUILDNR << 8) | REVNR
#define MIN_RAW_PULSES                    20                                    // =8 bits. Minimal number of bits*2 before decoding the signal.
#define RAWSIGNAL_SAMPLE_RATE             30                                    // Sample width / resolution in uSec for raw RF pulses.
#define MIN_PULSE_LENGTH                  25                                    // Pulses shorter than this value in uSec. will be seen as garbage and not taken as actual pulses.
#define MAX_PULSE_LENGTH                2000                                    // Pulses longer than this value in uSec. will be seen as garbage and not taken as actual pulses.
#define SIGNAL_TIMEOUT                     7                                    // Timeout, after this time in mSec. the RF signal will be considered to have stopped.
#define SIGNAL_REPEAT_TIME               500                                    // Time in mSec. in which the same RF signal should not be accepted again. Filters out retransmits.
#define BAUD                          115200                                    // Baudrate for serial communication.
#define TRANSMITTER_STABLE_DELAY         500                                    // delay to let the transmitter become stable (Note: Aurel RTX MID needs 500µS/0,5ms).
#define RAW_BUFFER_SIZE                  512                                    // Maximum number of pulses that is received in one go.
#define PLUGIN_MAX                        55                                    // Maximum number of Receive plugins
#define PLUGIN_TX_MAX                     26                                    // Maximum number of Transmit plugins
#define SCAN_HIGH_TIME                    50                                    // Time interval in ms. For background tasks fast processing
#define FOCUS_TIME                        50                                    // Duration in mSec. that, after receiving serial data from USB only the serial port is checked.
#define INPUT_COMMAND_SIZE               256                                    // Maximum number of characters that a command via serial can be.
#define PRINT_BUFFER_SIZE                256                                    // Maximum number of characters that a command should print in one go via the print buffer.
#define PRINT_BUFFER_SIZE                256                                    // Maximum number of characters that a command should print in one go via the print buffer.
#define VERSION_BUFFER_SIZE               16                                    // Maximum number of characters for version, build number string

/* Config file, here you can specify which plugins to load */
// #include "Config_01.c"

#define VALUE_PAIR                      44
#define VALUE_ALLOFF                    55
#define VALUE_OFF                       74
#define VALUE_ON                        75
#define VALUE_DIM                       76
#define VALUE_BRIGHT                    77
#define VALUE_UP                        78
#define VALUE_DOWN                      79
#define VALUE_STOP                      80
#define VALUE_CONFIRM                   81
#define VALUE_LIMIT                     82
#define VALUE_ALLON                     141

// PIN Definition
#define PIN_BSF_0                   22                                          // Board Specific Function lijn-0
#define PIN_BSF_1                   23                                          // Board Specific Function lijn-1
#define PIN_BSF_2                   24                                          // Board Specific Function lijn-2
#define PIN_BSF_3                   25                                          // Board Specific Function lijn-3
#define PIN_RF_TX_VCC               15                                          // +5 volt / Vcc power to the transmitter on this pin
#define PIN_RF_TX_DATA              14                                          // Data to the 433Mhz transmitter on this pin
#define PIN_RF_RX_VCC               16                                          // Power to the receiver on this pin
#define PIN_RF_RX_DATA              19                                          // On this input, the 433Mhz-RF signal is received. LOW when no signal.


// Function prptotypes
// ===================
void WifiStartup(void);
void WiFiRetryConnect(void);
void MQTTStartup(void);
void PluginInit(void);
void PluginTXInit(void);

boolean (*Plugin_ptr[PLUGIN_MAX])(byte, char*);                                 // Receive plugins
byte Plugin_id[PLUGIN_MAX];
boolean (*PluginTX_ptr[PLUGIN_TX_MAX])(byte, char*);                            // Transmit plugins
byte PluginTX_id[PLUGIN_TX_MAX];

void PrintHex16(unsigned int *data, uint8_t length);                            // prototype
void PrintHex8(uint8_t *data, uint8_t length);                                  // prototype
void PrintHexByte(uint8_t data);                                                // prototype
byte reverseBits(byte data);                                                    // prototype
void RFLinkHW( void );                                                          // prototype

void setupTime(void);

#endif //RF433_H
