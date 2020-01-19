#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DallasTemperature.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <WiFiClient.h>
#include <Wire.h>

#include "config.h"

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9

ESP8266WebServer server(80);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT);
Adafruit_MQTT_Publish pitFeed = Adafruit_MQTT_Publish(&mqtt, "prometheus/room/smoker/pitTemperature");
Adafruit_MQTT_Publish meatFeed = Adafruit_MQTT_Publish(&mqtt, "prometheus/room/smoker/meatTemperature");

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress pitProbe = LEFT_PLUG_ADDR;
DeviceAddress meatProbe = RIGHT_PLUG_ADDR;


String hexAddress(DeviceAddress deviceAddress)
{
    String out = "";
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16) out.concat("0");
        out.concat(String(deviceAddress[i], HEX));
    }
    return out;
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void handle_root() {
    sensors.requestTemperatures();
    float pitTemp = calibratePitTempF(sensors.getTempF(pitProbe));
    float meatTemp = calibrateMeatTempF(sensors.getTempF(meatProbe));

    String msg = "Pit probe goes on the left, meat probe goes on the right.\n\n";
    if (isnan(pitTemp)) {
        msg.concat("Pit Probe is disconnected.");
    } else {
        msg.concat("Pit Temp (°F): ");
        msg.concat(String(pitTemp));
    }

    msg.concat("\n");

    if (isnan(meatTemp)) {
        msg.concat("Meat Probe is disconnected.");
    } else {
        msg.concat("Meat Temp (°F): ");
        msg.concat(String(meatTemp));
    }

    msg.concat("\n");
    server.send(200, "text/plain;charset=UTF-8", msg);
}


void setup() {
    Serial.begin(115200);

    IPAddress staticIP(192,168,1,225);
    IPAddress gateway(192,168,1,1);
    IPAddress subnet(255,255,255,0);

    WiFi.config(staticIP, gateway, subnet);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("\n\r \n\rWorking to connect");
    int t1 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    int t2 = millis();
    Serial.println("");
    Serial.print("Connected to ");
    Serial.print(WIFI_SSID);
    Serial.print(" in ");
    Serial.print(t2 - t1);
    Serial.println(" milliseconds.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    // Start wifi server
    server.on("/", handle_root);
    server.begin();

    sensors.begin();
    sensors.setResolution(pitProbe, TEMPERATURE_PRECISION);
    sensors.setResolution(meatProbe, TEMPERATURE_PRECISION);

    // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(" devices.");
    Serial.print("Device 0 Address: ");
    printAddress(meatProbe);
    Serial.println();

    Serial.print("Device 1 Address: ");
    printAddress(pitProbe);
    Serial.println();
}


int period = 5000;
int tick = 0;

void loop() {
    int now = millis();
    server.handleClient();

    // Only update once every period
    if (now - tick > period) {
        tick = now;
        update();
    }
}

float calibratePitTempF(float pitTemp) {
    if (isnan(pitTemp)) {
        return pitTemp;
    }
    return map(pitTemp, 41, 362, 33, 360) - 2;
}

float calibrateMeatTempF(float meatTemp) {
    if (isnan(meatTemp)) {
        return meatTemp;
    }
    return map(meatTemp, 35, 365, 33, 360) + 2;
}

void update() {
    if (MQTT_connect() == 0) {
        sensors.requestTemperatures();
        float pitTemp = calibratePitTempF(sensors.getTempF(pitProbe));
        float meatTemp = calibrateMeatTempF(sensors.getTempF(meatProbe));
        pitFeed.publish(pitTemp);
        meatFeed.publish(meatTemp);
    }
}

int MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return 0;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(1500 * (4-retries));
       retries--;
       if (retries == 0) {
           return 1;
       }
  }

  Serial.println("MQTT Connected!");
  return 0;
}
