/**
 * MIT License
 * Copyright (c) 2019 Tim Huetz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// #define NDEBUG // if defined, we run in release mode
#define FORCE_HARD_RESTART_INSTEAD_DEEPSLEEP

#include "weather_station/SecureOTA.h"
#include "weather_station/root_cert.h"
#include "weather_station/version.h"
#include "weather_station/wifi.h"
#include <Adafruit_BME280.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <pins_arduino.h>

const uint16_t MAX_RAW_VOLTAGE = 810;
const uint16_t MIN_RAW_VOLTAGE = 600;
const uint8_t MAX_WIFI_CONNECTION_TRIES = 20;
const int32_t DEEP_SLEEP_SECONDS = 600;

unsigned long int measureRawBatteryVoltage() { return analogRead(A0); }

float calculateBatteryChargeInPercent(const float raw_voltage) {
	const float max_range = MAX_RAW_VOLTAGE - MIN_RAW_VOLTAGE;
	float percentage = ((raw_voltage - MIN_RAW_VOLTAGE) / max_range) * 100.0f;

	if (percentage > 100.0f) {
		percentage = 100.0f;
	}

	if (percentage < 0.0f) {
		percentage = 0.0f;
	}

	return percentage;
}

void updateLocalTime() {
	const long int localTimeZone = 8 * 3600;
	const int daylightOffsetInSeconds = 0;

	// try to update the time of the board using some NTP servers
#if !defined(NDEBUG)
	Serial.print("Setting time using NTP");
#endif
	configTime(localTimeZone, daylightOffsetInSeconds, "pool.ntp.org", "time.nist.gov");
	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2) {
		delay(500);
		Serial.print(".");
		now = time(nullptr);
	}
#if !defined(NDEBUG)
	Serial.println("");
#endif

// if we are in debug mode, show the updated local time in a human readable form
#if !defined(NDEBUG)
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	Serial.print("Current time: ");
	Serial.print(asctime(&timeinfo));
#endif
}

void sendMeasurements(float temp, float humidity, float pressure, float raw_voltage) {
	const String postUrl = ENDPOINT_BASE;
	HTTPClient http;
	BearSSL::WiFiClientSecure client;

	// get the id of the chip for authentication purposes
	char tmp[9];
	sprintf(tmp, "%08X", ESP.getChipId());

	// get the version of the current firmare
	char version_str[13]; // would be able to store "10.10.10-dev\0" so should be enough
	memset(version_str, 0, sizeof(char) * 13);
#if !defined(NDEBUG)
	sprintf(version_str, "%d.%d.%d-dev", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
#else
	sprintf(version_str, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
#endif

	// be sure that the CA certificate could be loaded successfully, otherwise we have to stop here
	const bool caCertSuccessfullyLoaded = client.setCACert_P(caCert, caCertLen);
	if (!caCertSuccessfullyLoaded) {
#if !defined(NDEBUG)
		Serial.println("Failed to load root CA certificate!");
#endif
		while (true) {
			yield();
		}
	}

	//
	const float charge = calculateBatteryChargeInPercent(raw_voltage);

	//
	String postData = "{\"temperature\":";
	postData += String(temp);
	postData += ",\"humidity\":";
	postData += String(humidity);
	postData += ",\"pressure\":";
	postData += String(pressure);
	postData += ",\"raw_voltage\":";
	postData += String(raw_voltage);
	postData += ",\"charge\":";
	postData += String(charge);
	postData += ",\"sensor\":\"";
	postData += String(tmp);
	postData += "\",\"firmware_version\":\"";
	postData += String(version_str);
	postData += "\"}";

	//
	http.begin(client, postUrl);
	http.addHeader("Content-Type", "application/json");
	const int httpCode = http.POST(postData);
#if !defined(NDEBUG)
	if (204 != httpCode) {
		Serial.printf("%d - Could not send temperature to endpoint.", httpCode);
		Serial.println();
	}
#endif

	//
	http.end();
	client.stop();
}

void measureAndShowValues() {
	Adafruit_BME280 bme;
	bool bme_status;
	bme_status = bme.begin(0x76); // address either 0x76 or 0x77
	if (!bme_status) {
#if !defined(NDEBUG)
		Serial.printf("Could not find a valid BME280 sensor, check wiring!");
		Serial.println();
#endif
		return;
	}

	bme.setSampling(Adafruit_BME280::MODE_FORCED,
					Adafruit_BME280::SAMPLING_X1, // temperature
					Adafruit_BME280::SAMPLING_X1, // pressure
					Adafruit_BME280::SAMPLING_X1, // humidity
					Adafruit_BME280::FILTER_OFF);

	bme.takeForcedMeasurement();

	// get the measurements
	const float measured_temp = bme.readTemperature();
	const float measured_humi = bme.readHumidity();
	const float measured_pres = bme.readPressure() / 100.0f;
	const float raw_voltage = measureRawBatteryVoltage();

	// ensure that we do not send inaccurate measurements which are caused by a too low voltage
	if (MIN_RAW_VOLTAGE >= raw_voltage) {
#if !defined(NDEBUG)
		Serial.printf("Not sending last measurement since the raw_voltage (%.2f) droped to or below %d", raw_voltage, MIN_RAW_VOLTAGE);
		Serial.println();
#endif
		return;
	}

	// send it
	sendMeasurements(measured_temp, measured_humi, measured_pres, raw_voltage);
}

void setup() {
	int connectionTries = 0;

	// connect D0 to RST to ensure we can wakeup after deep sleep
	pinMode(D0, WAKEUP_PULLUP);

#if !defined(NDEBUG)
	// setup the serial interface with a specified baud rate
	Serial.begin(115200);

	// just print a simple header
	Serial.println();
	Serial.printf("Solar Powered Weather Station %d.%d.%d - Written by Tim Huetz. All rights reserved.\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	Serial.printf("================================================================================");
	Serial.println();
#endif

	// try to connect to the wifi
	WiFi.hostname(WIFI_HOST);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		connectionTries++;
		delay(500);
		if (connectionTries > MAX_WIFI_CONNECTION_TRIES) {
#if !defined(NDEBUG)
			Serial.println();
			Serial.printf("Could not connect after %d tries, resetting and starting from the beginning...", connectionTries);
#endif
			ESP.reset();
		}
	}

	// ensure the local time is up to date (required for SSL cert validation)
	updateLocalTime();

	// do the actual measurements and send the values to a server
	measureAndShowValues();

#if !defined(NDEBUG)
	Serial.printf("Going to sleep for %d seconds now!", DEEP_SLEEP_SECONDS);
	Serial.println();
#endif

#if !defined(FORCE_HARD_RESTART_INSTEAD_DEEPSLEEP)
	// go into deep sleep mode to save energy
	ESP.deepSleep(DEEP_SLEEP_SECONDS * 1000000);
#else
	ESP.reset();
#endif
}

void loop() {
	// since we use deep-sleep, we will never reach this place
	// everything has to be done in the setup method
}
