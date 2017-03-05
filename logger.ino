
/* Copyright 2017 Julian Ecknig. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>

#define MAGIC_BYTES_CONFIGURED 1455243984 //If this value is saved in the EEPROM, we know it contains proper configuration instead of random garbage;

#define LEN_SSID 32
#define LEN_WIFI_PASSWD 32
#define LEN_MQTT_HOST 64
#define LEN_MQTT_USER 32
#define LEN_MQTT_PASSWD 32
#define LEN_MQTT_TOPIC 64

const char *ap_ssid = "ESP8266Test";
const char *ap_passwd = "nilpferd";

ESP8266WebServer server ( 80 );
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

String mqttClientId;

u32 magicBytes;

struct {
  char ssid[LEN_SSID + 1];
  char wifi_passwd[LEN_WIFI_PASSWD + 1];
  char mqtt_host[LEN_MQTT_HOST + 1];
  u16 mqtt_port;
  char mqtt_user[LEN_MQTT_USER + 1];
  char mqtt_passwd[LEN_MQTT_PASSWD + 1];
  char mqtt_topic[LEN_MQTT_TOPIC + 1];
  u8 measurement_pin;
  u32 measurement_delay;
} config;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(sizeof(magicBytes) + sizeof(config));
  delay(10);
  
  EEPROM.get(0, magicBytes);

  if (magicBytes == MAGIC_BYTES_CONFIGURED) {
    EEPROM.get(sizeof(magicBytes), config);
    connectToAp(config.ssid, config.wifi_passwd);
    pubSubClient.setServer(config.mqtt_host, config.mqtt_port);
    mqttClientId = getMqttClientId();
  } else {
    startAP(ap_ssid, ap_passwd);
    startConfigServer();  
  }
}

void loop() {
  if (magicBytes == MAGIC_BYTES_CONFIGURED) {
    logValue(analogRead(config.measurement_pin));
    delay(config.measurement_delay);
  } else {
    server.handleClient();
  }
}

void startAP(const char *ssid, const char *passwd) {
  //Default IP is 192.168.4.1
  Serial.print("Starting AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, passwd);
  Serial.println(" done");
}

void startConfigServer() {
  Serial.print("Starting HTTP server...");
  server.on ("/", handleRoot);
  server.begin();
  Serial.println(" done");
  Serial.println("Connect via http://192.168.4.1/"); 
}

void handleRoot() {
  switch (server.method()) {
    case HTTP_GET:
      server.send(200, "text/html",
"<html>\
  <body>\
    <form action=\"/\" method=\"post\" enctype=\"multipart/form-data\">\
      <h3>WiFi</h3>\
      <table>\
        <tbody>\
          <tr>\
            <td>\
              <label for=\"ssid\">SSID</label>\
            </td>\
            <td>\
              <input type=\"text\" id=\"ssid\" name=\"ssid\" maxlength=\"32\" required/>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"wifi_passwd\">Password</label>\
            </td>\
            <td>\
              <input type=\"password\" id=\"wifi_passwd\" name=\"wifi_passwd\" maxlength=\"32\" required/>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
      <h3>MQTT</h3>\
      <table>\
        <tbody>\
          <tr>\
            <td>\
              <label for=\"mqtt_host\">Hostname / IP-Address</label>\
            </td>\
            <td>\
              <input type=\"text\" id=\"mqtt_host\" name=\"mqtt_host\" maxlength=\"64\" required/>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"mqtt_port\">Port</label>\
            </td>\
            <td>\
              <input type=\"number\" id=\"mqtt_port\" name=\"mqtt_port\" min=\"1\" max=\"65535\" value=\"1883\" required/>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"mqtt_user\">Username</label>\
            </td>\
            <td>\
              <input type=\"text\" id=\"mqtt_user\" name=\"mqtt_user\" maxlength=\"32\"/>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"mqtt_passwd\">Password</label>\
            </td>\
            <td>\
              <input type=\"password\" id=\"mqtt_passwd\" name=\"mqtt_passwd\" maxlength=\"32\"/>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"mqtt_topic\">Topic</label>\
            </td>\
            <td>\
              <input type=\"text\" id=\"mqtt_topic\" name=\"mqtt_topic\" maxlength=\"32\" required/>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
      <h3>Measurement</h3>\
      <table>\
        <tbody>\
          <tr>\
            <td>\
              <label for=\"pin\">Pin number</label>\
            </td>\
            <td>\
              <input type=\"number\" id=\"measurement_pin\" name=\"measurement_pin\"/ min=\"0\" max=\"16\" required>\
            </td>\
          </tr>\
          <tr>\
            <td>\
              <label for=\"measurement_delay\">Delay (ms)</label>\
            </td>\
            <td>\
              <input type=\"number\" id=\"measurement_delay\" name=\"measurement_delay\"/ min=\"1\" max=\"4294967295\" required>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
      <input type=\"submit\" value=\"Speichern\"/>\
    </form>\
  </body>\
</html>");
      break;
      
    case HTTP_POST:
        strncpy(config.ssid, server.arg("ssid").c_str(), LEN_SSID);
        config.ssid[LEN_SSID] = '\0';
        strncpy(config.wifi_passwd, server.arg("wifi_passwd").c_str(), LEN_WIFI_PASSWD);
        config.wifi_passwd[LEN_WIFI_PASSWD] = '\0';
        strncpy(config.mqtt_host, server.arg("mqtt_host").c_str(), LEN_MQTT_HOST);
        config.mqtt_host[LEN_MQTT_HOST] = '\0';
        config.mqtt_port = server.arg("mqtt_port").toInt();
        strncpy(config.mqtt_user, server.arg("mqtt_user").c_str(), LEN_MQTT_USER);
        config.mqtt_user[LEN_MQTT_USER] = '\0';
        strncpy(config.mqtt_passwd, server.arg("mqtt_passwd").c_str(), LEN_MQTT_PASSWD);
        config.mqtt_passwd[LEN_MQTT_PASSWD] = '\0';
        strncpy(config.mqtt_topic, server.arg("mqtt_topic").c_str(), LEN_MQTT_TOPIC);
        config.mqtt_topic[LEN_MQTT_TOPIC] = '\0';
        config.measurement_pin = server.arg("measurement_pin").toInt();
        config.measurement_delay = server.arg("measurement_delay").toInt();

        magicBytes = MAGIC_BYTES_CONFIGURED;
        EEPROM.put(0, magicBytes);
        EEPROM.put(sizeof(magicBytes), config);
        EEPROM.commit();

        server.send(200, "text/html",
"<html>\
  <body>\
    Configuration has been saved. Starting logger.\
  </body>\
</html>");

        ESP.reset();
      break;
      
    default:
      server.send(405, "text/plain", "Method not allowed");
  }
}

void connectToAp(const char *ssid, const char *passwd) {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
   
  WiFi.begin(ssid, passwd);
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

String getMqttClientId() {
  String clientId = "ESP8266-";
  u8 mac[6];
  
  WiFi.macAddress(mac);
  
  for (int i = 5; i >= 0; i--) {
    clientId += String(mac[i], HEX);
    if (i != 0) {
      clientId += ":";
    }
  }

  return clientId;
}

void checkMqttConnection() {
  while (!pubSubClient.connected()) {
    Serial.println("Connecting to MQTT-Server...");
    if (pubSubClient.connect(mqttClientId.c_str(), config.mqtt_user, config.mqtt_passwd)) {
      Serial.println("Connected");
    } else {
      Serial.print("Couldn't connect. Failure state: ");
      Serial.print(pubSubClient.state());
      Serial.println(". retrying in 5s");
      delay(5000);
    }
  }
}

void logValue(int value) {
  checkMqttConnection();
  pubSubClient.loop();
  String strValue = String(value);
  pubSubClient.publish(config.mqtt_topic, strValue.c_str(), true);
  Serial.println(String(config.mqtt_topic) + ": " + strValue);
}
