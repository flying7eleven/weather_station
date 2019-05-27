#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#include "weather_station/wifi.h"
#include <Adafruit_BME280.h>
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

void measureAndShowValues() {
  Adafruit_BME280 bme;
  bool bme_status;
  bme_status = bme.begin(0x76);  //address either 0x76 or 0x77
  if (!bme_status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                Adafruit_BME280::SAMPLING_X1, // temperature
                Adafruit_BME280::SAMPLING_X1, // pressure
                Adafruit_BME280::SAMPLING_X1, // humidity
  Adafruit_BME280::FILTER_OFF );

  bme.takeForcedMeasurement();

  // Get temperature
  float measured_temp = bme.readTemperature();
  measured_temp = measured_temp + 0.0f;
  // print on serial monitor
  Serial.print("Temp: ");
  Serial.print(measured_temp);
  Serial.print("°C; ");

  // Get humidity
  float measured_humi = bme.readHumidity();
  // print on serial monitor
  Serial.print("Humidity: ");
  Serial.print(measured_humi);
  Serial.print("%; ");

  // Get pressure
  float measured_pres = bme.readPressure() / 100.0F;
  // print on serial monitor
  Serial.print("Pressure: ");
  Serial.print(measured_pres);
  Serial.print("hPa; ");

  // Show the ChipID / Sensor ID
  Serial.printf("ChipID: %08X;", ESP.getChipId());

  // new line
  Serial.println();
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
  measureAndShowValues();
  delay(5000);
}
