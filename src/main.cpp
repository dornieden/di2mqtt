/*
  ESP8266/ESP32 publish the RSSI as the WiFi signal strength to ThingSpeak channel.
  This example is for explaining how to use the AutoConnect library.

  In order to execute this example, the ThingSpeak account is needed. Sing up
  for New User Account and create a New Channel via My Channels.
  For details, please refer to the project page.
  https://hieromon.github.io/AutoConnect/howtoembed.html#used-with-mqtt-as-a-client-application

  Also, this example uses AutoConnectAux menu customization which stored in SPIFFS.
  To evaluate this example, you upload the contents as mqtt_setting.json of
  the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu
  in Arduino IDE).

  This example is based on the thinkspeak.com environment as of Dec. 20, 2018.
  Copyright (c) 2020 Hieromon Ikasamo.
  This software is released under the MIT License.
  https://opensource.org/licenses/MIT
*/

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define GET_CHIPID()  (ESP.getChipId())
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#define GET_CHIPID()  ((uint16_t)(ESP.getEfuseMac()>>32))
#endif
#include <PubSubClient.h>
#include <AutoConnect.h>

/*
  AC_USE_SPIFFS indicates SPIFFS or LittleFS as available file systems that
  will become the AUTOCONNECT_USE_SPIFFS identifier and is exported as showng
  the valid file system. After including AutoConnect.h, the Sketch can determine
  whether to use FS.h or LittleFS.h by AUTOCONNECT_USE_SPIFFS definition.
*/
#include <FS.h>
#if defined(ARDUINO_ARCH_ESP8266)
#ifdef AUTOCONNECT_USE_SPIFFS
FS& FlashFS = SPIFFS;
#else
#include <LittleFS.h>
FS& FlashFS = LittleFS;
#endif
#elif defined(ARDUINO_ARCH_ESP32)
#include <SPIFFS.h>
fs::SPIFFSFS& FlashFS = SPIFFS;
#endif

#define PARAM_FILE      "/param.json"
#define AUX_MQTTSETTING "/mqtt_setting"
#define AUX_MQTTSAVE    "/mqtt_save"
#define AUX_MQTTCLEAR   "/mqtt_clear"

// Adjusting WebServer class with between ESP8266 and ESP32.
#if defined(ARDUINO_ARCH_ESP8266)
typedef ESP8266WebServer  WiFiWebServer;
#elif defined(ARDUINO_ARCH_ESP32)
typedef WebServer WiFiWebServer;
#endif

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
String  serverName;
String  serverPort;
String  userName;
String  userPassword;
String  apid;
unsigned int  updateInterval = 60;
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

int getStrength(uint8_t points) {
  uint8_t sc = points;
  long    rssi = 0;

  while (sc--) {
    rssi += WiFi.RSSI();
    delay(20);
  }
  return points ? static_cast<int>(rssi / points) : 0;
}

String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = FlashFS.open(PARAM_FILE, "r");
  if (param) {
    aux.loadElement(param);
    param.close();
  }
  else
    Serial.println(PARAM_FILE " open failed");
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
  if (updateInterval > 0) {
    if (millis() - lastPub > updateInterval) {
      if (!mqttClient.connected()) {
        mqttConnect();
      }
      String item = String("field1=") + String(getStrength(7));
      mqttPublish(item);
      mqttClient.loop();
      lastPub = millis();
    }
  }
}
