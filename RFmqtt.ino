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

void(* resetFunc) (void) = 0; //declare reset function @ address

void MQTTStartup() {
  MQTTClient.setServer(MQTT_broker_hostname, MQTT_port);
  MQTTClient.setCallback(MQTTcallback);

  // Wait until connected to MQTT Broker
  // client.connect returns a boolean value
  Serial.println(F("Connecting to MQTT broker ..."));
  // Poll until connected.
  while (!sendMQTTStatus())
    ;

// Subscribe to the TOPICs

  Serial.println(F("Subscribing to MQTT topics ..."));
  for (int i = 0; i < MAXLISTEN; i++) {
    Serial.print(i+1);
    Serial.print(F(" - "));
    Serial.println(mqtt_listen_array[i]);
    do {
      MQTTClient.loop();
      Serial.print(F(" .. subscribing to "));
      Serial.println(mqtt_listen_array[i]);
      delay(100);
    } while (!MQTTClient.subscribe(mqtt_listen_array[i]));
  }
}

boolean sendMQTTStatus(void) {

  StaticJsonBuffer<128> RF_topic_status;                   // Status data
  JsonObject& status_message = RF_topic_status.createObject();

// Publish current status

  currentTime = now();

  status_message["time"] = currentTime;                     // epoch
  status_message["ID"] = (F("20;00;Nodo RadioFrequencyLink - RFLink MQTT Gateway V1.1"));
  sprintf(vbuffer, "%04x", RFLINK_FW_VERSION);
  Serial.println(vbuffer);
  
  status_message["FW"] = vbuffer;

  status_message.printTo(MQTTOutput, 512);
  strcpy(mqtt_topic, mqtt_topic_status);            // Generic status response topic
  strcat(mqtt_topic, hostString);                   // Add the NodeId

  Serial.print(mqtt_topic);
  Serial.print(F(" : "));
  Serial.println(MQTTOutput);

  // PUBLISH to the MQTT Broker
  if (MQTTClient.publish(mqtt_topic, MQTTOutput)) {
    return true;
  }
  // If the message failed to send, try again, as the connection may have broken.
  else {
    Serial.println(F("Status Message failed to publish. Reconnecting to MQTT Broker and trying again .. "));
    if (MQTTClient.connect(clientID, MQTT_broker_username, MQTT_broker_password)) {
      Serial.println(F("reconnected to MQTT Broker!"));
      delay(100); // This delay ensures that client.publish doesn't clash with the client.connect call
      if (MQTTClient.publish(mqtt_topic_status, MQTTOutput)) {
        Serial.println(F("Status Message published after one retry."));
        return true;
      }
      else {
        Serial.println(F("Status Message failed to publish after one retry."));
        return false;
      }
    }
    else {
      Serial.println(F("Connection to MQTT Broker failed..."));
      return false;
    }
  }
}

boolean sendMQTTMessage(char* pbuffer) {

  StaticJsonBuffer<128> RF_topic_message;                   // message data
  JsonObject& message = RF_topic_message.createObject();

// Publish current message
  currentTime = now();

  message["time"] = currentTime;                     // epoch
  message["PCL"] = SignalHash;                       // protocol
  message["MSG"] = pbuffer;                          // data

  message.printTo(MQTTOutput, 512);
  strcpy(mqtt_topic, mqtt_topic_message);           // Generic message response topic
  strcat(mqtt_topic, hostString);                   // Add the NodeId

  Serial.print(mqtt_topic);
  Serial.print(F(" : "));
  Serial.println(MQTTOutput);

  // PUBLISH to the MQTT Broker
  if (MQTTClient.publish(mqtt_topic, MQTTOutput)) {
    return true;
  }
  // If the message failed to send, try again, as the connection may have broken.
  else {
    Serial.println(F("Status Message failed to publish. Reconnecting to MQTT Broker and trying again .. "));
    if (MQTTClient.connect(clientID, MQTT_broker_username, MQTT_broker_password)) {
      Serial.println(F("reconnected to MQTT Broker!"));
      delay(100); // This delay ensures that client.publish doesn't clash with the client.connect call
      if (MQTTClient.publish(mqtt_topic_status, MQTTOutput)) {
        Serial.println(F("Status Message published after one retry."));
        return true;
      }
      else {
        Serial.println(F("Status Message failed to publish after one retry."));
        return false;
      }
    }
    else {
      Serial.println(F("Connection to MQTT Broker failed..."));
      return false;
    }
  }
}


boolean sendMQTTException(int AssetIdx, int Number) {
  publishJSON(mqtt_topic_exception);
}

boolean publishJSON(const char* topic) {
// PUBLISH to the MQTT Broker
  if (MQTTClient.publish(topic, MQTTOutput)) {
    return true;
  }
// If the message failed to send, try again, as the connection may have broken.
  else {
    Serial.println(F("Send Message failed. Reconnecting to MQTT Broker and trying again .. "));
    if (MQTTClient.connect(clientID, MQTT_broker_username, MQTT_broker_password)) {
      Serial.println(F("reconnected to MQTT Broker!"));
      delay(100); // This delay ensures that client.publish doesn't clash with the client.connect call
      if (MQTTClient.publish(topic, MQTTOutput)) {
        return true;
      }
      else {
        Serial.println(F("Send Message failed after one retry."));
        return false;
      }
    }
    else {
      Serial.println(F("Connection to MQTT Broker failed..."));
      return false;
    }
  }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {

  int i;
  const char* Node = "*";     // Preset for anyone
  const char* Command = " ";  // Command to execute
  char* RF_topic;             // Variable to hold all node topics
  int data;                   // Data for output
  boolean succeed;

  RF_topic = &MQTTOutput[0];
  StaticJsonBuffer<200> RF_topic_command;            // Parsing buffer

  Serial.println(topic);
// Copy topic to char* buffer
  for(i = 0; i < length; i++){
    MQTTInput[i] = (char)payload[i];
    Serial.print(MQTTInput[i]);
  }
  MQTTInput[i] = 0x00;                  // Null terminate buffer to use string functions
  Serial.println();

//Parse the topic data
  JsonObject& command_topic = RF_topic_command.parseObject(MQTTInput);
  if (!command_topic.success())
  {
    return;
  }

}
