#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <ArduinoOTA.h>
#include <DS3231.h>
#include <EmonLib.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <Wire.h>

// Ezzel könnyebb lesz majd állítani a maximum áramerősséget.
#define AMPERAGE_MAX 1050

// The date responsible for the file naming which is holding the data.
int display_date;

// Clock hardware module.
DS3231 Clock;

// Webserver setup for networking.
WebServer Server(80);

struct TimeClock
{
	bool century, format_12, pm;
} time_clock;

struct Sensor
{
	EnergyMonitor emon;
	double irms;
} sensors[3];

struct CollectedData
{
	float min, max, sum, cnt, time;
} collected_data;

struct ParsableData
{
	float min, max, avg;
} parsable_data[24];

int GetCurrentDate()
{
	char get_date[9];
	sprintf(get_date, "%04d%02d%02d", 2000 + Clock.getYear(), Clock.getMonth(time_clock.century), Clock.getDate());

	return String(get_date).toInt();
}

// On connect, the device would recieve the HTML data.
void HandleOnConnect()
{
	Serial.println("Connection handling called.");

	SetCurrentDataTable();					// Aligns the data for the HTML page.
	MakeIndexPage();						// Making HTML page.
	StreamFile("/index.html", "text/html"); // Streaming HTML.
}

// If the server was not found, the device would return 404.
void HandleNotFound()
{
	Serial.println("Error: 404 (Not found)");
	Server.send(404, "text/plain", "Not found");
}

void StreamFile(const char *path_to_file, String mime_type)
{
	// Streaming the generated html file, if it exists.
	if (SPIFFS.exists(path_to_file))
	{
		File file_to_stream = SPIFFS.open(path_to_file, "r");
		Server.streamFile(file_to_stream, mime_type);
		file_to_stream.close();
	}
	else
		HandleNotFound();
}

void HandleToday()
{
	Serial.println("Handling current day record.");
	display_date = GetCurrentDate();
	HandleOnConnect();
}

void HandlePreviousDay()
{
	Serial.println("Handling previous day record.");
	GetPreviousDate();
	HandleOnConnect();
}

void HandleNextDay()
{
	Serial.println("Handling next day record.");
	GetNextDate();
	HandleOnConnect();
}

void HandleDump()
{
	Serial.println("CSV Dump");
	StreamFile(GetDataFileName().c_str(), "text/plain");
}

void NetworkInitialization()
{
	// const char *WIFI_SSID = "KT Partners";
	const char *WIFI_SSID = "KT Partners";
	const char *WIFI_PASSWORD = "felavilagtetejeremama";

	Serial.println("Initializing network...");
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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

	if (WiFi.status() == WL_CONNECTED)
		Serial.println(WiFi.localIP());
	else
		Serial.println(WiFi.softAPIP());

	Server.on("/", HandleToday);
	Server.on("/next", HandleNextDay);
	Server.on("/prev", HandlePreviousDay);
	Server.on("/data", HandleDump);

	Server.onNotFound(HandleNotFound);
	Server.begin();
	Serial.println("Server started.");
}

// ? Might be able to remove?
void ClockInitialization()
{
	Serial.println("Initializing time...");

	for (int i = 0; i < 5; i++)
	{
		delay(1000);
		Serial.print(Clock.getYear(), DEC);
		Serial.print(".");
		Serial.print(Clock.getMonth(time_clock.century), DEC);
		Serial.print(".");
		Serial.print(Clock.getDate(), DEC);
		Serial.print(". ");
		Serial.print(Clock.getHour(time_clock.format_12, time_clock.pm), DEC);
		Serial.print(":");
		Serial.print(Clock.getMinute(), DEC);
		Serial.print(":");
		Serial.println(Clock.getSecond(), DEC);
	}

	display_date = GetCurrentDate();
}

void GetPreviousDate()
{
	for (size_t i = display_date - 1; i > 20201013; i--)
	{
		File data_file = SPIFFS.open(GetDataFileName(), FILE_READ);

		// * Debugging
		Serial.print("Looking for file: ");
		Serial.println(GetDataFileName());

		if (data_file)
		{
			display_date = i;
			Serial.print("File found: ");
			Serial.println(GetDataFileName());
			break;
		}
	}
}

void GetNextDate()
{
	for (size_t i = display_date + 1; i <= GetCurrentDate(); i++)
	{
		File data_file = SPIFFS.open(GetDataFileName(), FILE_READ);

		// * Debugging
		Serial.print("Looking for file: ");
		Serial.println(GetDataFileName());

		if (data_file)
		{
			display_date = i;
			Serial.print("File found: ");
			Serial.println(GetDataFileName());
			break;
		}
	}
}

void GetCurrentData()
{
	if (collected_data.time == Clock.getMinute())
		return;

	collected_data.time = Clock.getMinute();

	for (size_t i = 0; i < *(&sensors + 1) - sensors; i++)
	{
		sensors[i].irms = random(1, 350); // * sensors[i].emon.calcIrms(1480);

		collected_data.min = sensors[i].irms < collected_data.min ? sensors[i].irms : collected_data.min;
		collected_data.max = sensors[i].irms > collected_data.max ? sensors[i].irms : collected_data.max;
		collected_data.sum += sensors[i].irms;
	}

	if (collected_data.cnt == 5)
	{
		writeDataToCSV(collected_data.min, collected_data.max, collected_data.sum / collected_data.cnt);

		collected_data.min = AMPERAGE_MAX;
		collected_data.max = 0;
		collected_data.sum = 0;
		collected_data.cnt = 0;
	}

	collected_data.cnt++;
	Serial.println("Gotten data by the minute.");
}

void SetCurrentDataTable()
{
	memset(parsable_data, 0, sizeof(parsable_data));

	struct TemporaryData
	{
		String ptr = "";
		float min = AMPERAGE_MAX;
		float max, avg, time, cnt = 1;
	} temporary_data;

	File data_file = SPIFFS.open(GetDataFileName());
	if (!data_file)
		return;

	while (data_file.available())
	{
		char fileRead = data_file.read();

		if (fileRead == '\n')
		{
			// * 0020.10.08. 13:07,0000,0000,0000

			if (temporary_data.time == temporary_data.ptr.substring(12, 14).toInt())
			{
				temporary_data.min = temporary_data.ptr.substring(18, 22).toInt() < temporary_data.min ? temporary_data.ptr.substring(18, 22).toInt() : temporary_data.min;
				temporary_data.max = temporary_data.ptr.substring(23, 27).toInt() > temporary_data.max ? temporary_data.ptr.substring(23, 27).toInt() : temporary_data.max;
				temporary_data.avg = round(temporary_data.ptr.substring(28, 32).toInt() / temporary_data.cnt);
				temporary_data.cnt++;

				parsable_data[temporary_data.ptr.substring(12, 14).toInt()].min = temporary_data.min;
				parsable_data[temporary_data.ptr.substring(12, 14).toInt()].max = temporary_data.max;
				parsable_data[temporary_data.ptr.substring(12, 14).toInt()].avg = temporary_data.avg;
			}
			else
				temporary_data.time = temporary_data.ptr.substring(12, 14).toInt();

			temporary_data.ptr = "";
		}
		else
			temporary_data.ptr += fileRead;
	}

	Serial.println("Data has been set.");
}

String GetDataFileName()
{
	String file_name = "/";
	file_name += display_date;
	file_name += ".csv";

	return file_name;
}

void writeDataToCSV(int min, int max, int avg)
{
	struct DateTime
	{
		int year, month, date, hour, minute;
	} date_time;

	// File holder for the main data file.
	File file = SPIFFS.open(GetDataFileName(), FILE_APPEND);

	if (!file)
	{
		Serial.println("Failed to open file for writing.");
		return;
	}

	char data_log[33] = "";

	date_time.year = Clock.getYear();
	date_time.month = Clock.getMonth(time_clock.century);
	date_time.date = Clock.getDate();
	date_time.hour = Clock.getHour(time_clock.format_12, time_clock.pm);
	date_time.minute = Clock.getMinute();

	// Sor formázás
	sprintf(data_log, "%04d.%02d.%02d. %02d:%02d,%04d,%04d,%04d", 2000 + date_time.year, date_time.month, date_time.date, date_time.hour, date_time.minute, min, max, avg);

	if (!file.println(data_log))
		Serial.println("Writing failed.");

	Serial.println(data_log);
	file.close();
}

String fileToString(const char *file_path)
{
	File file_to_convert = SPIFFS.open(file_path);
	if (!file_to_convert)
	{
		Serial.println("Failed to open file. (File to String)");
		return "";
	}

	String s = "";
	while (file_to_convert.available())
	{
		char fileRead = file_to_convert.read();
		s += fileRead;
	}

	return s;
}

void generateChart(File html_file)
{
	for (size_t i = 0; i < *(&parsable_data + 1) - parsable_data; i++)
	{
		parsable_data[i].min = parsable_data[i].min == 0 ? 1 : parsable_data[i].min;
		parsable_data[i].max = parsable_data[i].max == 0 ? 1 : parsable_data[i].max;
		parsable_data[i].avg = parsable_data[i].avg == 0 ? 1 : parsable_data[i].avg;

		html_file.print("<div class='bar1' style='--bar-value:");
		html_file.print(round(parsable_data[i].max / 1050 * 100));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(parsable_data[i].max));
		html_file.print("'>");

		html_file.print("<div class='bar2' style='--bar-value:");
		html_file.print(round(parsable_data[i].avg / parsable_data[i].max * 100));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(parsable_data[i].avg));
		html_file.print("'>");

		html_file.print("<div class='bar3' style='--bar-value:");
		html_file.print(round(parsable_data[i].min / parsable_data[i].avg * 100));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(parsable_data[i].min));
		html_file.print("'>");

		html_file.print("</div></div></div>");
	}
}

// Imports the external CSS from SPIFFS.
void MakePageHead(File html_file)
{
	html_file.print("<head>");
	html_file.print("<style>");
	html_file.print(fileToString("/style.css"));
	html_file.print("</style>");
	html_file.print("</head>");
}

// Makes the chart for in the body.
void MakePageBody(File html_file)
{
	html_file.print("<body>");
	html_file.print("<table class='graph'>");
	html_file.print("<h1 style=\"margin-left: 10px; font-family: sans-serif; font-size: 25px; font-weight: bold;\">");
	html_file.print(display_date);
	html_file.print(".");
	html_file.print("</h1>");
	html_file.print("<div class='chart-wrap vertical'><div class='grid'>");

	generateChart(html_file);

	html_file.print("</div>");
	html_file.print("<div class=\"footr\">");
	html_file.print("<button onclick=\"location.href='/prev'\" class='button button1' style=\"transform: translateX(20px);\">Elozo</button>");
	html_file.print("<button onclick=\"location.href='/'\" class='button button1' style=\"transform: translateX(70px);\">Mai nap</button>");
	html_file.print("<button onclick=\"location.href='/next'\" class='button button1' style=\"transform: translateX(120px);\">Kovetkezo</button>");
	html_file.print("<button onclick=\"location.href='/data'\" class='button2' style=\"transform: translateX(550px);\">Data</button>");
	html_file.print("</div></div></div>");
	html_file.print("</body>");
}

void MakeIndexPage()
{
	Serial.println("Attempting to make HTML page.");

	File html_file = SPIFFS.open("/index.html", FILE_WRITE);
	if (!html_file)
	{
		Serial.println("Error making HTML page. (SPIFFS).");
		return;
	}

	// Making the HTML page functionally.
	html_file.print("<!DOCTYPE html>");
	MakePageHead(html_file);
	MakePageBody(html_file);

	html_file.close();
}

void OtaInitialization()
{
	ArduinoOTA
		.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println("Start updating " + type);
		})
		.onEnd([]() {
			Serial.println("\nEnd");
		})
		.onProgress([](unsigned int progress, unsigned int total) {
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		})
		.onError([](ota_error_t error) {
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR)
				Serial.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR)
				Serial.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR)
				Serial.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR)
				Serial.println("Receive Failed");
			else if (error == OTA_END_ERROR)
				Serial.println("End Failed");
		});

	ArduinoOTA.begin();
}

// Init
void setup()
{
	Serial.begin(115200);

	if (!SPIFFS.begin())
		Serial.println("Failed SPIFFS inizialization.");

	Wire.begin();

	NetworkInitialization();
	ClockInitialization();
	OtaInitialization();

	sensors[0].emon.current(32, 10);
	sensors[1].emon.current(33, 10);
	sensors[2].emon.current(35, 10);
}

// Process
void loop()
{
	Server.handleClient();
	ArduinoOTA.handle();
	GetCurrentData();
}
