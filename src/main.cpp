/**	
* di2mqtt main file.
* @file main.cpp
* @author info@smartpassivehouse.com
* @version 0.0.1
* @date 2021-02-25
* @copyright MIT license.
*/

#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <AutoConnect.h>
#include <FS.h>
#include <SPIFFS.h>
fs::SPIFFSFS& FlashFS = SPIFFS;

#define PARAM_FILE      "/param.json"
#define AUX_MQTTSETTING "/mqtt_setting"
#define AUX_MQTTSAVE    "/mqtt_save"
#define AUX_MQTTCLEAR   "/mqtt_clear"

// ----------------------------------------------------------------------
// configure Sensors
#define SENSOR_COUNT 20
#define DEVICE_NAME "di2mqtt"
#define SW_VERSION "1.1.2"

byte sensorPins[SENSOR_COUNT] = { 
  4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39 
};

String deviceName = DEVICE_NAME;

String sensorNames[SENSOR_COUNT] = {
  "input_1", // PIN IO4 on ESP32
  "input_2", // PIN IO5 on ESP32
  "input_3", // PIN IO13 on ESP32
  "input_4", // PIN IO14 on ESP32
  "input_5", // PIN IO16 on ESP32
  "input_6", // PIN IO17 on ESP32
  "input_7", // PIN IO18 on ESP32
  "input_8", // PIN IO19 on ESP32
  "input_9", // PIN IO21 on ESP32
  "input_10", // PIN IO22 on ESP32
  "input_11", // PIN IO23 on ESP32
  "input_12", // PIN IO25 on ESP32
  "input_13", // PIN IO26 on ESP32
  "input_14", // PIN IO27 on ESP32
  "input_15", // PIN IO32 on ESP32
  "input_16", // PIN IO33 on ESP32
  "input_17", // PIN IO34 on ESP32
  "input_18", // PIN IO35 on ESP32
  "input_19", // PIN Sensor_VP on ESP32
  "input_20"  // PIN Sensor_VN on ESP32
};

boolean sensorStates[SENSOR_COUNT] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

// Adjusting WebServer class with between ESP8266 and ESP32.
typedef WebServer WiFiWebServer;

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
String  serverName;
String  serverPort;
String  userName;
String  userPassword;
String  apid;
unsigned int  updateInterval = 60000;
unsigned long lastPub = 0;
unsigned long previousMillis = 0;
unsigned long interval = 30000;

bool mqttConnect() {
  uint8_t retry = 3;
  while (!mqttClient.connected()) {
    if (serverName.length() <= 0)
      break;

    mqttClient.setServer(serverName.c_str(), serverPort.toInt());
    Serial.println(String("Attempting MQTT broker:") + serverName);


    if (mqttClient.connect( "di2mqtt", userName.c_str(), userPassword.c_str() )) {
      Serial.println("Established:" );
      mqttClient.publish("di2mqtt/debug/status", "Connected to MQTT");
      return true;
    } else {
      Serial.println("Connection failed:" + String(mqttClient.state()));
      if (!--retry)
        break;
      delay(3000);
    }
  }
  return false;
}

void mqttPublish(String msg) {
  String path = String("channels/") + String("/publish/");
  mqttClient.publish(path.c_str(), msg.c_str());
}

String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = FlashFS.open(PARAM_FILE, "r");
  if (param) {
    // Load the elements with parameters
    bool rc = aux.loadElement(param);
    if (rc) {
      // here, obtain parameters
      serverName = aux.getElement<AutoConnectInput>("mqttserver").value;
      serverPort = aux.getElement<AutoConnectInput>("mqttport").value;
      userName = aux.getElement<AutoConnectInput>("mqttuser").value;
      userPassword = aux.getElement<AutoConnectInput>("mqttpassword").value;

      Serial.println(String(PARAM_FILE) + " loaded");
  }
  else
    Serial.println(PARAM_FILE " open failed");
  }
  param.close();
  return String("");
}

String saveParams(AutoConnectAux& aux, PageArgument& args) {
  serverName = args.arg("mqttserver");
  serverName.trim();

  serverPort = args.arg("mqttport");
  serverPort.trim();
  
  userName = args.arg("mqttuser");
  userName.trim();
  
  userPassword = args.arg("mqttpassword");
  userPassword.trim();

  
  // The entered value is owned by AutoConnectAux of /mqtt_setting.
  // To retrieve the elements of /mqtt_setting, it is necessary to get
  // the AutoConnectAux object of /mqtt_setting.
  File param = FlashFS.open(PARAM_FILE, "w");
  portal.aux("/mqtt_setting")->saveElement(param, { "mqttserver", "mqttport", "mqttuser", "mqttpassword" });
  param.close();

  // Echo back saved parameters to AutoConnectAux page.
  AutoConnectText&  echo = aux["parameters"].as<AutoConnectText>();
  echo.value = "Server: " + serverName + "<br>";
  echo.value += "Port: " + serverPort + "<br>";
  echo.value += "Username: " + userName + "<br>";
  echo.value += "Password: " + userPassword + "<br>";

  return String("");
}

void handleRoot() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<meta http-equiv=\"refresh\" content=\"5\">"
    "</head>"
    "<body>"
    "<p style=\"padding-top:5px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "<H1>Digital Input States</H1>"
    "<p>Input_1: {{INPUT1}}</p>"
    "<p>Input_2: {{INPUT2}}</p>"
    "<p>Input_3: {{INPUT3}}</p>"
    "<p>Input_4: {{INPUT4}}</p>"
    "<p>Input_5: {{INPUT5}}</p>"
    "<p>Input_6: {{INPUT6}}</p>"
    "<p>Input_7: {{INPUT7}}</p>"
    "<p>Input_8: {{INPUT8}}</p>"
    "<p>Input_9: {{INPUT9}}</p>"
    "<p>Input_10: {{INPUT10}}</p>"
    "<p>Input_11: {{INPUT11}}</p>"
    "<p>Input_12: {{INPUT12}}</p>"
    "<p>Input_13: {{INPUT13}}</p>"
    "<p>Input_14: {{INPUT14}}</p>"
    "<p>Input_15: {{INPUT15}}</p>"
    "<p>Input_16: {{INPUT16}}</p>"
    "<p>Input_17: {{INPUT17}}</p>"
    "<p>Input_18: {{INPUT18}}</p>"
    "<p>Input_19: {{INPUT19}}</p>"
    "<p>Input_20: {{INPUT20}}</p>"
    "<H1>Debug Info</H1>"
    "<p>SW-Version: {{SW_VERSION}}</p>"
    "</body>"
    "</html>";

  content.replace( "{{INPUT1}}", String( digitalRead(sensorPins[0]) ) );
  content.replace( "{{INPUT2}}", String( digitalRead(sensorPins[1]) ) );
  content.replace( "{{INPUT3}}", String( digitalRead(sensorPins[2]) ) );
  content.replace( "{{INPUT4}}", String( digitalRead(sensorPins[3]) ) );
  content.replace( "{{INPUT5}}", String( digitalRead(sensorPins[4]) ) );
  content.replace( "{{INPUT6}}", String( digitalRead(sensorPins[5]) ) );
  content.replace( "{{INPUT7}}", String( digitalRead(sensorPins[6]) ) );
  content.replace( "{{INPUT8}}", String( digitalRead(sensorPins[7]) ) );
  content.replace( "{{INPUT9}}", String( digitalRead(sensorPins[8]) ) );
  content.replace( "{{INPUT10}}", String( digitalRead(sensorPins[9]) ) );
  content.replace( "{{INPUT11}}", String( digitalRead(sensorPins[10]) ) );
  content.replace( "{{INPUT12}}", String( digitalRead(sensorPins[11]) ) );
  content.replace( "{{INPUT13}}", String( digitalRead(sensorPins[12]) ) );
  content.replace( "{{INPUT14}}", String( digitalRead(sensorPins[13]) ) );
  content.replace( "{{INPUT15}}", String( digitalRead(sensorPins[14]) ) );
  content.replace( "{{INPUT16}}", String( digitalRead(sensorPins[15]) ) );
  content.replace( "{{INPUT17}}", String( digitalRead(sensorPins[16]) ) );
  content.replace( "{{INPUT18}}", String( digitalRead(sensorPins[17]) ) );
  content.replace( "{{INPUT19}}", String( digitalRead(sensorPins[18]) ) );
  content.replace( "{{INPUT20}}", String( digitalRead(sensorPins[19]) ) );
  content.replace( "{{SW_VERSION}}", String( SW_VERSION ) );


  WiFiWebServer&  webServer = portal.host();
  webServer.send(200, "text/html", content);
}

bool loadAux(const String auxName) {
  bool  rc = false;
  String  fn = auxName + ".json";
  File fs = FlashFS.open(fn.c_str(), "r");
  if (fs) {
    rc = portal.load(fs);
    fs.close();
  }
  else
    Serial.println("Filesystem open failed: " + fn);
  return rc;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  FlashFS.begin(true);
  loadAux(AUX_MQTTSETTING);
  loadAux(AUX_MQTTSAVE);
  config.ota = AC_OTA_BUILTIN;

  AutoConnectAux* setting = portal.aux(AUX_MQTTSETTING);
  if (setting) {
    PageArgument  args;
    AutoConnectAux& mqtt_setting = *setting;
    loadParams(mqtt_setting, args);

    config.homeUri = "/";

    portal.on(AUX_MQTTSETTING, loadParams);
    portal.on(AUX_MQTTSAVE, saveParams);
  }
  else
    Serial.println("aux. load error");

  // Reconnect and continue publishing even if WiFi is disconnected.
  config.autoReconnect = true;
  config.reconnectInterval = 1;
  portal.config(config);

  Serial.print("WiFi ");
  if (portal.begin()) {
    config.bootUri = AC_ONBOOTURI_HOME;
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }

  // sensors
  for (byte i = 0; i < SENSOR_COUNT; i++) {
      byte pin = sensorPins[i];
      pinMode(pin, INPUT);
  }

  WiFiWebServer&  webServer = portal.host();
  webServer.on("/", handleRoot);
}

void loop() {
  portal.handleClient();
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();

  // if WiFi is down, try reconnecting
  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  
  // send alive message including sw-version
  if (updateInterval > 0) {
    if ( (millis() - lastPub > updateInterval) || (millis() < lastPub) ) {
      mqttClient.publish("di2mqtt/debug/alive", "1");
      mqttClient.publish("di2mqtt/debug/version", SW_VERSION);
      lastPub = millis();
    }
  }

// sensor logic
  for (byte i = 0; i < SENSOR_COUNT; i++) {
        
        // Create some helper variables.
        byte pin = sensorPins[i];
        boolean pinState = digitalRead(pin);
        boolean lastPinState =  sensorStates[i];
        String sensorName = sensorNames[i];

      if (pinState != lastPinState) {

          // Define a string with the topic to which we want to post the new state to ...
          // and convert it into a char * array which we need for the pubsubclient.
          String feedNameString =  String(deviceName + "/status/" + sensorName);
          char topic[feedNameString.length() + 1 ];
          feedNameString.toCharArray(topic, feedNameString.length() + 1);

          // Output the new state to the serial for debugging.
          Serial.print("New state for sensor ");
          Serial.print(topic);
          Serial.print(": ");
          Serial.print(pinState);

          // Publish the new state to the MQTT server.
          // The message is sent as a retained message to always 
          // have the last state available on the broker.
          // The QoS is set to 1, to make sure the delivery 
          // of the mseeage to the broker is guaranteed.            
          if (mqttClient.publish(topic, pinState ? "1" : "0") ){
              Serial.println(F(" ... Published!"));
          } else {
              Serial.println(F(" ... MQTT PUBLISH FAILED!"));
          }
          
          // Store the new pinstate in the pinStates array.
          sensorStates[i] = pinState;
      }
  }

  // delay at the end of loop
  delay(50);
}
