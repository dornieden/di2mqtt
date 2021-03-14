/**	
* di2mqtt main file.
* @file main.cpp
* @author info@smartpassivehouse.com
* @version 0.0.1
* @date 2021-02-25
* @copyright MIT license.
*/

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
unsigned int  updateInterval = 600000;
unsigned long lastPub = 0;

bool mqttConnect() {
  uint8_t retry = 3;
  while (!mqttClient.connected()) {
    if (serverName.length() <= 0)
      break;

    mqttClient.setServer(serverName.c_str(), serverPort.toInt());
    Serial.println(String("Attempting MQTT broker:") + serverName);


    if (mqttClient.connect( "di2mqtt", userName.c_str(), userPassword.c_str() )) {
      Serial.println("Established:" );
      mqttClient.publish("di2mqtt/debug", "Connected to MQTT");
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
    "</head>"
    "<body>"
    "<p style=\"padding-top:5px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";

  WiFiWebServer&  webServer = portal.host();
  webServer.send(200, "text/html", content);
}


// Load AutoConnectAux JSON from the flash on the board.
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

  WiFiWebServer&  webServer = portal.host();
  webServer.on("/", handleRoot);
}

void loop() {
  portal.handleClient();
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();
  
  // send alive message
  if (updateInterval > 0) {
    if ( (millis() - lastPub > updateInterval) || (millis() < lastPub) ) {
      mqttClient.publish("di2mqtt/debug", "alive");
      lastPub = millis();
    }
  }
  // delay at the end of loop
  delay(50);
}
