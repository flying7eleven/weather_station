#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#include "weather_station/wifi.h"
#include <ESP8266WiFi.h>

void(* resetFunc) (void) = 0;

void indicateStillConnecting() {
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, LOW);
  delay(100);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(100);
}

void indicateConnected() {
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, LOW);
  delay(2000);
  digitalWrite(BUILTIN_LED, HIGH);
}

void setup() {
  int connectionTries = 0;

  // setup the serial interface with a specified baud rate
  Serial.begin(115200);

  // just print a simple header
  Serial.println();
  Serial.printf("Solar Powered Weather Station %d.%d.%d - Written by Tim Huetz. All rights reserved.\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  Serial.printf("================================================================================");
  Serial.println();

  // try to connect to the wifi
  WiFi.hostname(WIFI_HOST);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)  {
    connectionTries++;
    indicateStillConnecting();
    delay(500);
    if (connectionTries > 20) {
      Serial.println();
      Serial.printf("Could not connect after %d tries, resetting and starting from the beginning...", connectionTries);
      resetFunc();
    }
  }
  indicateConnected();
}

void loop() {
  Serial.printf("Chip ID = %08X\n", ESP.getChipId());
  Serial.println("");
  WiFi.printDiag(Serial);
  delay(5000);
}
