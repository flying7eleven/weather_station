
#include "weather_station/version.h"
#include "weather_station/wifi.h"
#include <Adafruit_BME280.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <RemoteDebug.h>

void (*resetFunc)(void) = 0;

RemoteDebug Debug;

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

float measureBatteryVoltage() {
	const float calib_factor = 5.28f; // change this value to calibrate the battery voltage
	unsigned long raw = analogRead(A0);
	return raw * calib_factor / 1024.0f;
}

void sendMeasurements(float temp, float humidity, float pressure, float voltage) {
	char tmp[9];
	sprintf(tmp, "%08X", ESP.getChipId());
	HTTPClient http;
	String postUrl = "https://weather.huetz.biz/v1/measurement/";
	String postData = "{\"temperature\":";
	postData += String(temp);
	postData += ",\"humidity\":";
	postData += String(humidity);
	postData += ",\"pressure\":";
	postData += String(pressure);
	postData += ",\"voltage\":\"";
	postData += String(voltage);
	postData += ",\"sensor\":\"";
	postData += String(tmp);
	postData += "\"}";
	http.begin(postUrl);
	http.addHeader("Content-Type", "application/json");
	int httpCode = http.POST(postData);
	if (204 != httpCode) {
		debugW("%d - Could not send temperature to endpoint.", httpCode);
	}
	http.end();
}

void measureAndShowValues() {
	Adafruit_BME280 bme;
	bool bme_status;
	bme_status = bme.begin(0x76); // address either 0x76 or 0x77
	if (!bme_status) {
		debugE("Could not find a valid BME280 sensor, check wiring!");
		return;
	}

	bme.setSampling(Adafruit_BME280::MODE_FORCED,
					Adafruit_BME280::SAMPLING_X1, // temperature
					Adafruit_BME280::SAMPLING_X1, // pressure
					Adafruit_BME280::SAMPLING_X1, // humidity
					Adafruit_BME280::FILTER_OFF);

	bme.takeForcedMeasurement();

	// Get temperature
	float measured_temp = bme.readTemperature();
	measured_temp = measured_temp + 0.0f;
	debugI("Temperature: %d °C", measured_temp);

	// Get humidity
	float measured_humi = bme.readHumidity();
	debugI("Humidity: %d %%", measured_humi);

	// Get pressure
	float measured_pres = bme.readPressure() / 100.0f;
	debugI("Pressure: %d hPa", measured_pres);

	// Show the current battery voltage
	float volts = measureBatteryVoltage();
	debugI("Battery voltate: %.2f Volts", volts);

	// Show the ChipID / Sensor ID
	debugI("ChipID: %08X;", ESP.getChipId());

	// send it
	sendMeasurements(measured_temp, measured_humi, measured_pres, volts);
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
	while (WiFi.status() != WL_CONNECTED) {
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

	//
	Debug.begin(WIFI_HOST);
	Debug.showColors(true);
}

void loop() {
	measureAndShowValues();
	Debug.handle();
	delay(5000);
}
