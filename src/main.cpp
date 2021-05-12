/**
 * MIT License
 * Copyright (c) 2019 - 2021 Tim Huetz
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

#include <Adafruit_BME280.h>
#include <Arduino_JSON.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <pins_arduino.h>

#include "version.h"
#include "wifi.h"

const uint8_t WIFI_CHANNEL = 6;
const uint16_t MAX_RAW_VOLTAGE = 814;
const uint16_t MIN_RAW_VOLTAGE = 605;
const uint8_t MAX_WIFI_CONNECTION_TRIES = 20;
const int32_t DEEP_SLEEP_SECONDS = 60 * 15;    // sleep for 15 Minutes after each measurement
const uint8_t BME260_ONE_WIRE_ADDRESS = 0x76;  // address either 0x76 or 0x77
const uint32_t MICROSECONDS_PER_SECOND = 1000000;
const uint16_t WAIT_FOR_WIFI_IN_MILLISECONDS = 500;

auto measureRawBatteryVoltage() -> float {
    return static_cast<float>(analogRead(A0));
}

auto calculateBatteryChargeInPercent(const float raw_voltage) -> float {
    const float max_range = MAX_RAW_VOLTAGE - MIN_RAW_VOLTAGE;
    float percentage = ((raw_voltage - MIN_RAW_VOLTAGE) / max_range) * 100.0F;

    if (percentage > 100.0F) {
        percentage = 100.0F;
    }

    if (percentage < 0.0F) {
        percentage = 0.0F;
    }

    return percentage;
}

auto sendMeasurements(const char* chipId, float temp, float humidity, float pressure, float raw_voltage) -> void {
    HTTPClient http;
    BearSSL::WiFiClientSecure client;
    client.setInsecure();

    // get the version of the current firmware
    const uint8_t VERSION_STRING_LENGTH = 13;
    std::array<char, VERSION_STRING_LENGTH> version_str{};  // would be able to store "10.10.10-dev\0" so should be enough
#if !defined(NDEBUG)
    snprintf(version_str.data(), VERSION_STRING_LENGTH, "%d.%d.%d-dev", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
#else
    snprintf(version_str.data(), VERSION_STRING_LENGTH, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
#endif

    //
    const float charge = calculateBatteryChargeInPercent(raw_voltage);

    //
    JSONVar jsonData;

    //
    jsonData["temperature"] = temp;
    jsonData["humidity"] = humidity;
    jsonData["pressure"] = pressure;
    jsonData["raw_voltage"] = raw_voltage;
    jsonData["charge"] = charge;
    jsonData["sensor"] = chipId;
    jsonData["firmware_version"] = version_str.data();

    //
    String postData = JSON.stringify(jsonData);
#if !defined(NDEBUG)
    Serial.printf("%s", postData.c_str());
    Serial.println();
#endif

    //
    http.begin(ENDPOINT_HOST, ENDPOINT_PORT, ENDPOINT_PATH);
    http.setUserAgent("WeatherStation/BA188");
    http.addHeader("Content-Type", "application/json");
    const uint32_t httpCode = http.POST(postData);
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
    bool bme_status = false;
    bme_status = bme.begin(BME260_ONE_WIRE_ADDRESS);
    if (!bme_status) {
#if !defined(NDEBUG)
        Serial.printf("Could not find a valid BME280 sensor, check wiring!");
        Serial.println();
#endif
        return;
    }

    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,  // temperature
                    Adafruit_BME280::SAMPLING_X1,  // pressure
                    Adafruit_BME280::SAMPLING_X1,  // humidity
                    Adafruit_BME280::FILTER_OFF);

    bme.takeForcedMeasurement();

    // get the measurements
    const float measured_temp = bme.readTemperature();
    const float measured_humi = bme.readHumidity();
    const float measured_pres = bme.readPressure() / 100.0F;
    const float raw_voltage = measureRawBatteryVoltage();

    // ensure that we do not send inaccurate measurements which are caused by a too low voltage
    if (MIN_RAW_VOLTAGE >= raw_voltage) {
#if !defined(NDEBUG)
        Serial.printf("Not sending last measurement since the raw_voltage (%.2f) dropped to or below %d", raw_voltage, MIN_RAW_VOLTAGE);
        Serial.println();
#endif
        return;
    }

    // get the id of the chip for authentication purposes
    const uint8_t CHIP_ID_STRING_LENGTH = 9;
    std::array<char, CHIP_ID_STRING_LENGTH> tmp{};
    snprintf(tmp.data(), CHIP_ID_STRING_LENGTH, "%08X", ESP.getChipId());

    // send it
    sendMeasurements(tmp.data(), measured_temp, measured_humi, measured_pres, raw_voltage);
}

void setup() {
    int connectionTries = 0;

    // connect D0 to RST to ensure we can wakeup after deep sleep
    pinMode(D0, WAKEUP_PULLUP);

#if !defined(NDEBUG)
    // setup the serial interface with a specified baud rate
    Serial.begin(115200);
#endif

    // try to connect to the wifi
    WiFi.hostname(WIFI_HOST);
    WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
    while (WiFi.status() != WL_CONNECTED) {
        connectionTries++;
        delay(WAIT_FOR_WIFI_IN_MILLISECONDS);
        if (connectionTries > MAX_WIFI_CONNECTION_TRIES) {
            ESP.reset();
        }
    }
#if !defined(NDEBUG)
    Serial.printf("Connected to the WiFi after %d tries...", connectionTries);
    Serial.println();
#endif

    // do the actual measurements and send the values to a server
    measureAndShowValues();

    // disconnect from the network before proceeding
    WiFi.disconnect(true);

#if !defined(NDEBUG)
    Serial.printf("Going to sleep for %d seconds now!", DEEP_SLEEP_SECONDS);
    Serial.println();
#endif

    // go into deep sleep mode to save energy
    ESP.deepSleep(DEEP_SLEEP_SECONDS * MICROSECONDS_PER_SECOND);
}

void loop() {
    // since we use deep-sleep, we will never reach this place
    // everything has to be done in the setup method
}
