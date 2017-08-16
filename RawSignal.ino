#include <Arduino.h>

/*********************************************************************************************/
boolean ScanEvent(void) {                                         // Deze routine maakt deel uit van de hoofdloop en wordt iedere 125uSec. doorlopen
  unsigned long EventTimer = millis() + SCAN_HIGH_TIME;           // SCAN_HIGH_TIME is 50mS

  while (EventTimer > millis() || RepeatingTimer > millis()) {

       if (FetchSignal(PIN_RF_RX_DATA, HIGH)) {                   // RF: Try to capture a packet of data into the buffer      
          if ( PluginRXCall(0,0) ) {                              // If valid, check all plugins to see which plugin can handle the received signal.
             RepeatingTimer = millis() + SIGNAL_REPEAT_TIME;      // Extend timer (SIGNAL_REPEAT_TIME is 500mS) .. ?? since it immediately returns ??
             return true;
          }
       }
  } // while
  return false;
}
/**********************************************************************************************\
 * Haal de pulsen en plaats in buffer. 
 * bij de TSOP1738 is in rust is de uitgang hoog. StateSignal moet LOW zijn
 * bij de 433RX is in rust is de uitgang laag. StateSignal moet HIGH zijn
 * 
 \*********************************************************************************************/
#ifndef ESP32
const unsigned long LoopsPerMilli = 345;
#endif
#ifdef ESP32
const unsigned long LoopsPerMilli = 15000;       // ESP32 does 15k loops in 1 mS!
#endif
const unsigned long Overhead = 0;  

// Because this is a time critical routine, we use global variables so that the variables 
// do not need to be initialized at each function call. 
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

/*********************************************************************************************/
boolean FetchSignal(byte DataPin, boolean StateSignal) {
   long Fbit = digitalPinToBitMask(DataPin);
   int  Fport = digitalPinToPort(DataPin);
   long FstateMask = (StateSignal ? Fbit : 0);

// Try to find the unchanging end of the RF message i.e. no transitions
//  and then look for the start transition of the next (maybe first) message

// METHOD
// ======
// 1. Check for retransmit timeout of messages, including any repeats.
// 2. If the signal level has not changed, extend the signal timeout ( to maximum of  retransmit timeout)
// 3. If the signal level has changed, wait for retransmit timeout

// 1.
if ((*portInputRegister(Fport) & Fbit) == FstateMask) {
     if (RawSignal.Time) {
        if (RawSignal.Repeats && (RawSignal.Time + SIGNAL_REPEAT_TIME) > millis()) {
          PulseLength = micros() + SIGNAL_TIMEOUT * 1000;
// 2.          
          while ( (PulseLength > micros()) && ((RawSignal.Time + SIGNAL_REPEAT_TIME) > millis()) )
            if ((*portInputRegister(Fport) & Fbit) == FstateMask)
              PulseLength = micros() + SIGNAL_TIMEOUT * 1000;
// 3.
          while ( ((*portInputRegister(Fport) & Fbit) != FstateMask) && ((RawSignal.Time + SIGNAL_REPEAT_TIME) > millis()) )
            ;
        }
     }

// OK, found a valid start signal (maybe)
     RawCodeLength=1;                                                            // Start at 1 for legacy reasons. Element 0 can be used to pass special information like plugin number etc.
     Ftoggle=false;                                                              // Initialize signal state compare flag                  
     maxloops = (SIGNAL_TIMEOUT * LoopsPerMilli);                                // Set maximum loop count derived from signal timeout  
     numloops = 0;                                                               // Initialie loop counter
     startuS=micros();                                                           // Record first start time
     do {                                                                        // read the pulses in microseconds and place them in temporary buffer RawSignal

// Wait while signal is at the same level
       while ( ((*portInputRegister(Fport) & Fbit) == FstateMask) ^ Ftoggle )      // while() loop *A*
         if (numloops++ == maxloops) break;                                        // timeout
 
// Found change of signal level, so record end time (= start time of next!)
       finishuS = micros();
       PulseLength = finishuS - startuS;
       if (PulseLength < MIN_PULSE_LENGTH) break;                                  // Pulse length too short
       RawSignal.Pulses[RawCodeLength++] = PulseLength / RAWSIGNAL_SAMPLE_RATE;    // scale is 30 - store in RawSignal !!!! 

// Set up for next loop
       Ftoggle = !Ftoggle;    // Invert signal state compare
       startuS = finishuS;    // Set new start time
       numloops = 0;          // Initialize timeout

// until buffer is full, or signal level time-out, or a rogue pulse
    } while ( (RawCodeLength < RAW_BUFFER_SIZE) && (numloops <= maxloops) && (PulseLength < MAX_PULSE_LENGTH) );

    if (RawCodeLength >= MIN_RAW_PULSES) {
       RawSignal.Repeats = 0;                                                      // no repeats
       RawSignal.Multiply = RAWSIGNAL_SAMPLE_RATE;                                 // sample size.
       RawSignal.Number = RawCodeLength-1;                                         // Number of received pulse times (pulsen *2)
       RawSignal.Pulses[RawSignal.Number+1] = 0;                                   // Last element contains the timeout. 
       RawSignal.Time = millis();                                                  // Time the RF packet was received (to keep track of retransmits
       return true;
    } else {
      RawSignal.Number = 0;    
    }
  }
  return false;
}
/*********************************************************************************************/
// RFLink Board specific: Generate a short pulse to switch the Aurel Transceiver from TX to RX mode.
void RFLinkHW( void ) {
     delayMicroseconds(36);
     digitalWrite(PIN_BSF_0,LOW);
     delayMicroseconds(16);
     digitalWrite(PIN_BSF_0,HIGH);
     return;
}
/*********************************************************************************************\
 * Send rawsignal buffer to RF  * DEPRICATED * DO NOT USE *
\*********************************************************************************************/
void RawSendRF(void) {                                                    // * DEPRICATED * DO NOT USE *
  int x;
  digitalWrite(PIN_RF_RX_VCC,LOW);                                        // Spanning naar de RF ontvanger uit om interferentie met de zender te voorkomen.
  digitalWrite(PIN_RF_TX_VCC,HIGH);                                       // zet de 433Mhz zender aan
  delayMicroseconds(TRANSMITTER_STABLE_DELAY);                            // short delay to let the transmitter become stable (Note: Aurel RTX MID needs 500µS/0,5ms)

  RawSignal.Pulses[RawSignal.Number]=1;                                   // due to a bug in Arduino 1.0.1

  for(byte y=0; y<RawSignal.Repeats; y++) {                               // herhaal verzenden RF code
     x=1;
     noInterrupts();
     while(x<RawSignal.Number) {
        digitalWrite(PIN_RF_TX_DATA,HIGH);
        delayMicroseconds(RawSignal.Pulses[x++]*RawSignal.Multiply-5);    // min een kleine correctie  
        digitalWrite(PIN_RF_TX_DATA,LOW);
        delayMicroseconds(RawSignal.Pulses[x++]*RawSignal.Multiply-7);    // min een kleine correctie
     }
     interrupts();
     if (y+1 < RawSignal.Repeats) delay(RawSignal.Delay);                 // Delay buiten het gebied waar de interrupts zijn uitgeschakeld! Anders werkt deze funktie niet.
  }

  delayMicroseconds(TRANSMITTER_STABLE_DELAY);                            // short delay to let the transmitter become stable (Note: Aurel RTX MID needs 500µS/0,5ms)
  digitalWrite(PIN_RF_TX_VCC,LOW);                                        // zet de 433Mhz zender weer uit
  digitalWrite(PIN_RF_RX_VCC,HIGH);                                       // Spanning naar de RF ontvanger weer aan.
  RFLinkHW();
}
/*********************************************************************************************/

