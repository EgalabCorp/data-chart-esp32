/*
 Copyright (c) 2014-present PlatformIO <contact@platformio.org>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
**/

#include "SecureOTA.h"
#include <Arduino.h>
#include <DataChart.h>
#include <WiFi.h>

const uint16_t OTA_CHECK_INTERVAL = 3000; // ms
uint32_t _lastOTACheck = 0;

void setup()
{
	Serial.begin(115200);
	delay(10);

	Serial.print("Device version: v.");
	Serial.println(VERSION);
	Serial.println("Connecting to " + String(WIFI_SSID));

	WiFi.begin(WIFI_SSID, WIFI_PASS);

	int retries = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Establishing connection to WiFi...");

		retries++;

		if (retries >= 20)
		{
			const char *LOCAL_SSID = "ESP 32";
			const char *LOCAL_PASSWORD = "12345678";

			// Connection variable for the local network.
			IPAddress local_ip(10, 0, 0, 1);
			IPAddress gateway(10, 0, 0, 254);
			IPAddress subnet(255, 255, 255, 0);

			WiFi.softAP(LOCAL_SSID, LOCAL_PASSWORD);
			WiFi.softAPConfig(local_ip, gateway, subnet);
			delay(100);

			Serial.println("Failed connecting to outside network.");
			break;
		}
	}

	Serial.print("IP: ");
	if (WiFi.status() == WL_CONNECTED)
		Serial.println(WiFi.localIP());
	else
		Serial.println(WiFi.softAPIP());

	_lastOTACheck = millis();

	// your setup code goes here
}

void loop()
{
	if ((millis() - OTA_CHECK_INTERVAL) > _lastOTACheck)
	{
		_lastOTACheck = millis();
		checkFirmwareUpdates();
	}

	// your loop code goes here
	Serial.print("Date: ");
	Serial.println(getCurrentDate());
}
