/**
   IotWebConf06MqttApp.ino -- IotWebConf is an ESP8266/ESP32
     non blocking WiFi/AP web configuration library for Arduino.
     https://github.com/prampec/IotWebConf

   Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>

   This software may be modified and distributed under the terms
   of the MIT license.  See the LICENSE file for details.
*/

/**
   Example: MQTT Demo Application
   Description:
     All IotWebConf specific aspects of this example are described in
     previous examples, so please get familiar with IotWebConf before
     starting this example. So nothing new will be explained here,
     but a complete demo application will be built.
     It is also expected from the reader to have a basic knowledge over
     MQTT to understand this code.

     This example starts an MQTT client with the configured
     connection settings.
     Will post the status changes of the D2 pin in channel "test/status".
     Receives messages appears in channel "test/action", and writes them to serial.
     This example also provides the firmware update option.
     (See previous examples for more details!)

   Software setup for this example:
     This example utilizes Joel Gaehwiler's MQTT library.
     https://github.com/256dpi/arduino-mqtt

   Hardware setup for this example:
     - An LED is attached to LED_BUILTIN pin with setup On=LOW.
     - [Optional] A push button is attached to pin D2, the other leg of the
       button should be attached to GND.
*/

#include <MQTT.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <esp_task_wdt.h>

#define RXD2 16
#define TXD2 17
#define WDTO 4
#define LED  LED_BUILTIN
#define WDT_TIMEOUT 120

String topicBase = "lora/gw78/";

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "DevIOTA78";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "iotelkafi";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "mqt2"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN 4

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Method declarations.
void handleRoot();
void mqttMessageReceived(String &topic, String &payload);
bool connectMqtt();
bool connectMqttOptions();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
bool mqttCon = false;

DNSServer dnsServer;
WebServer server(80);
WiFiClient net;
MQTTClient mqttClient;

char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
char mqttUserTopicBaseValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
// -- You can also use namespace formats e.g.: iotwebconf::ParameterGroup
IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT configuration");
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfTextParameter mqttUserNameParam = IotWebConfTextParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfPasswordParameter mqttUserPasswordParam = IotWebConfPasswordParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN);
IotWebConfTextParameter mqttTopicBaseParam = IotWebConfTextParameter("Topic Base", "topicBase", mqttUserTopicBaseValue, STRING_LEN);

bool needMqttConnect = false;
bool needReset = false;
int pinState = HIGH;
int countMqttConnect = 0;
unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;
int wdtCount = 0;
unsigned long lastWdt = 0;
const int updateWdt = 60000;
const int needRstWdt = 20; // 20 menit tanpa koneksi

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println();
  Serial.println("Starting up...");
  pinMode(LED, OUTPUT);
  pinMode(WDTO, OUTPUT);
  digitalWrite(WDTO, LOW);

  mqttGroup.addItem(&mqttServerParam);
  mqttGroup.addItem(&mqttUserNameParam);
  mqttGroup.addItem(&mqttUserPasswordParam);
  mqttGroup.addItem(&mqttTopicBaseParam);

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameterGroup(&mqttGroup);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  // -- Initializing the configuration.
  bool validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
    mqttUserTopicBaseValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });

  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  Serial.println("Ready.");
}

void loop()
{
  // -- doLoop should be called as frequently as possible.
  esp_task_wdt_reset();

  iotWebConf.doLoop();
  mqttClient.loop();

  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient .connected()))
  {
    Serial.println("MQTT reconnect");
    connectMqtt();
  }

  long nowWdt = millis();
  if ((nowWdt - lastWdt) > updateWdt) {
    lastWdt = nowWdt;
    wdtCount++;
    if (wdtCount > needRstWdt) {
      needReset = true;
    }
  }
  

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  unsigned long now = millis();
  if ((5000 < now - lastReport) && (pinState != digitalRead(CONFIG_PIN)))
  {
    pinState = 1 - pinState; // invert pin state as it is changed
    lastReport = now;
    Serial.print("Sending on MQTT channel 'test/status' :");
    Serial.println(pinState == LOW ? "ON" : "OFF");
    mqttClient.publish("test/status", pinState == LOW ? "ON" : "OFF");
  }


  if (Serial2.available() > 0) {  // data from node sensor

    String str = Serial2.readStringUntil('$');
    Serial.println(str);
    if (mqttCon && (str.length() > 2)) {
      if (str.indexOf("GW")) {
        mqttClient.publish(topicBase + "data", str);
      } else {
        mqttClient.publish(topicBase + "sta", str);
      }
      blinkLed(2);
    }

  }
}

/**
   Handle web requests to "/" path.
*/
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 06 MQTT App</title></head><body>MQTT App demo";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += "</li>";
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  needMqttConnect = true;
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  needReset = true;
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form.");
  bool valid = true;

  int l = webRequestWrapper->arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  return valid;
}

bool connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  Serial.println("Connecting to MQTT server...");
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    countMqttConnect++;
    if (countMqttConnect > 100) {
      needReset = true;
    }
    return false;
  }
  countMqttConnect = 0;
  Serial.println("Connected!");
  mqttCon = true;
  mqttClient.publish(topicBase + "init", "online");
  mqttClient.subscribe(topicBase + "cmd");
  return true;
}

/*
  // -- This is an alternative MQTT connection method.
  bool connectMqtt() {
  Serial.println("Connecting to MQTT server...");
  while (!connectMqttOptions()) {
    iotWebConf.delay(1000);
  }
  Serial.println("Connected!");

  mqttClient.subscribe("test/action");
  return true;
  }
*/

bool connectMqttOptions()
{
  bool result;
  if (mqttUserPasswordValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
  }
  else if (mqttUserNameValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue);
  }
  else
  {
    result = mqttClient.connect(iotWebConf.getThingName());
  }
  if (mqttUserTopicBaseValue[0] != '\0') {
    topicBase = mqttUserTopicBaseValue;
  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{
  Serial.println("Incoming: " + topic + " - " + payload);
  if (payload.length() > 2) {
    Serial2.println(payload);
  } else {
    wdtCount = 0;
  }
}
