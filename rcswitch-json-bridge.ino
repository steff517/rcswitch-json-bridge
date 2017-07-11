#include <ArduinoJson.h>
#include <MQTTClient.h>

/*
 * Can receive commands through serial interface or WIFI (from MQTT broker) in JSON format. It will send those commands through 433MHz radio
 * Can receive 433MHz radio commands and sends them in JSON format through serial or WIFI to a MQTT broker. 
 *
 */

#include <ESP8266WiFi.h>
#include <SimpleDHT.h>
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();


/* 
 * WIFI Settings 
 */
const char* WIFI_SSID;  
const char* WIFI_PASSWORD;

/* 
 * MQTT Settings 
 */
 
// if not, only Serial will be used
int USE_WIFI_MQTT = 0;
const char* SERVER;
const char* MQTT_USER;
const char* MQTT_PASSWORD;

// messages on this topic will be converted and transmitted as 433MHz signal
const char* SEND_TOPIC;
// 433MHz signals are converted and published on this topic
const char* RECEIVE_TOPIC;

// a heartbeat singal is sent regularly on this topic
const char* HEARTBEAT_TOPIC;
long HEARTBEAT_INTERVAL = 60000; // 60s

// to identify this device 
const char* DEVICE_ID; 


unsigned long lastMillis = 0;
WiFiClient net;
MQTTClient client;

/*
 * Prints debug/info messages in JSON format
 */
void serialOut(String out) {
  Serial.println("{\"message\":"+out+"\"}");
}
void setup() {
  pinMode(LED_BUILTIN, OUTPUT); 
  Serial.begin(115200);
  serialOut("Booting");
  
  
  readConfig();
  
  mySwitch.enableTransmit(4); // NodeMCU Pin D2
  mySwitch.enableReceive(0);  // NodeMCU Pin D3
  
  delay(10);

  // PIN MODES
  pinMode(LED_BUILTIN, OUTPUT);
  if (USE_WIFI_MQTT) {
    serialOut("Connecting to WIFI and MQTT");
    client.begin(SERVER, net);
    initWifi();
    connectMQTT();
  } else {
    serialOut("Not using WIFI and MQTT");  
  }

  digitalWrite(LED_BUILTIN, HIGH); 
  
}

void readConfig() {
  WIFI_SSID = "SSID";
  WIFI_PASSWORD = "Password";
  SERVER = "iot.eclipse.org";  
  MQTT_USER = "try";
  MQTT_PASSWORD = "try";
  SEND_TOPIC = "send-as-radio";
  RECEIVE_TOPIC = "received-from-radio";
  HEARTBEAT_TOPIC = "heartbeat";
  DEVICE_ID = "Device 42";

  //todo: actually read the config e.g. from SD card
}



/*
 * Turn on WIFI and connect to SSID
 */
void initWifi() { 
  serialOut("Connecting to "+String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    serialOut("Still tying...");
  }
  serialOut("IP address: "+WiFi.localIP());  
}

/**
 * Turn off WIFI
 */
void wifiOff() {
  WiFi.mode(WIFI_OFF);
}

/**
 * connect to the MQTT Server
 */
void connectMQTT() {
  if(!client.connected()) {    
    
    serialOut("Connecting to MQTT Server: "+String(SERVER)+" ...");
    while (!client.connect("arduino", MQTT_USER, MQTT_PASSWORD)) {
      serialOut("Still trying...");
      delay(1000);
    }
    client.onMessage(messageReceived);
    client.subscribe(SEND_TOPIC);
    serialOut("Successfully connected to MQTT");
  } 
}

void loop() {

  if (USE_WIFI_MQTT) {
    client.loop();
  }
  check433Input();
  checkSerialInput();  
   
  if (millis() - lastMillis > HEARTBEAT_INTERVAL) {
    lastMillis = millis();
    if (USE_WIFI_MQTT) {
      client.publish(HEARTBEAT_TOPIC, String(millis()));
    }
  }  
}

/**
 * Check for data available on serial port and process it
 */
void checkSerialInput() {

   while (Serial.available() > 0) {   
      digitalWrite(LED_BUILTIN, LOW); 
      String sentence = Serial.readString();
      jsonReceived(sentence);
      digitalWrite(LED_BUILTIN, HIGH); 
   }
}

/**
 * Check for data available on the 433MHz radio receiver and process it
 */
void check433Input() {
  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    digitalWrite(LED_BUILTIN, LOW); 
    if (value == 0) {
      serialOut("Unknown encoding");
    } else {
      serialOut("Received 433MHz data");
      DynamicJsonBuffer jsonBuffer;

      JsonObject& root = jsonBuffer.createObject();
      root["time"] = millis();
      root["device_id"] = DEVICE_ID;
      root["protocol"] = mySwitch.getReceivedProtocol();     
      root["bits"] = mySwitch.getReceivedBitlength();
      root["pulse"] = mySwitch.getReceivedDelay();
      root["code"] = mySwitch.getReceivedValue();
      
      String json = "";
      root.printTo(json);
      Serial.println(json);
     
      if (USE_WIFI_MQTT) {
        client.publish(RECEIVE_TOPIC, json);
      }
    }
    mySwitch.resetAvailable();
    
  }
  digitalWrite(LED_BUILTIN, HIGH); 
}

/**
 * Sends the given JSON object on the 433MHz Radio interface
 */
void jsonReceived(String json) {

  
  Serial.println("Received json: "+json);
  DynamicJsonBuffer jsonBuffer;
  
  JsonObject& root = jsonBuffer.parseObject(json);
  
  int protocol  = root["protocol"];
  int repeat    = root["repeat"];
  int bits      = root["bits"];
  int pulse     = root["pulse"];
  long code     = root["code"];

  if (repeat == 0) {
    repeat = 5;
  } 
  
   // Optional set protocol (default is 1, will work for most outlets)
  mySwitch.setProtocol(protocol);  
  //Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(repeat);
  mySwitch.setPulseLength(pulse);  
  mySwitch.send(code,bits); 
  
}

/**
 * Receive and process MQTT message
 */
void messageReceived(String &topic, String &payload) {
 //todo: convert MQTT message to json
  
}
