#include "DataChart.h"

#define AMPERAGE_MAX 1200

void DataInit()
{
	InitializeTime();
	InitializeSensors();
	InitializeServer();
}

void DataProcess()
{
	GetChartData();
}

/*
* Nem működik az RTClib, vagy legalább is nem találja.
* Ha inicializálom, amikor elér az abort()-hoz, akkor a board
* újraindul. Ez a kód igazából az oldalon lévő example-ből épül
* fel, és nem változtattam rajta semmit, azon kívűl, hogy az egy-soros
* if statement körül kiszedtem a lezárásokat. Hozzáadtam a "Wire.h"
* libet, de még így se működik.
*/

DS3231 ds_clock;

ClockProperties clock_props;
int display_date = 20201014;

void InitializeTime()
{
	Serial.println("Initializing time...");

	for (int i = 0; i < 5; i++)
	{
		delay(1000);
		Serial.print(ds_clock.getYear(), DEC);
		Serial.print(".");
		Serial.print(ds_clock.getMonth(clock_props.century), DEC);
		Serial.print(".");
		Serial.print(ds_clock.getDate(), DEC);
		Serial.print(". ");
		Serial.print(ds_clock.getHour(clock_props.format_12, clock_props.pm), DEC);
		Serial.print(":");
		Serial.print(ds_clock.getMinute(), DEC);
		Serial.print(":");
		Serial.println(ds_clock.getSecond(), DEC);
	}

	display_date = GetCurrentDate();
}

int GetCurrentDate()
{
	char date_buf[9];
	sprintf(date_buf, "%04d%02d%02d", 2000 + ds_clock.getYear(), ds_clock.getMonth(clock_props.century), ds_clock.getDate());

	return String(date_buf).toInt();
}

ParsableData data_pack[24];

void GenerateChart(File html_file)
{
	for (size_t i = 0; i < *(&data_pack + 1) - data_pack; i++)
	{
		// The devider cannot be 0.
		data_pack[i].min = data_pack[i].min == 0 ? 1 : data_pack[i].min;
		data_pack[i].max = data_pack[i].max == 0 ? 1 : data_pack[i].max;
		data_pack[i].avg = data_pack[i].avg == 0 ? 1 : data_pack[i].avg;

		// Bar 3
		html_file.print("<div class='bar1' style='--bar-value:");
		html_file.print(round(data_pack[i].max / 12));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(data_pack[i].max));
		html_file.print("'>");

		// Bar 2
		html_file.print("<div class='bar2' style='--bar-value:");
		html_file.print(round(data_pack[i].avg / data_pack[i].max * 100));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(data_pack[i].avg));
		html_file.print("'>");

		// Bar 3
		html_file.print("<div class='bar3' style='--bar-value:");
		html_file.print(round(data_pack[i].min / data_pack[i].avg * 100));
		html_file.print("%;' data-name='");
		html_file.print(i);
		html_file.print("h' title='");
		html_file.print(String(data_pack[i].min));
		html_file.print("'>");

		// Close the class down.
		html_file.print("</div></div></div>");
	}
}

void GenerateStyle(File html_file)
{
	File css_file = SPIFFS.open("/style.css");
	if (!css_file) return;

	while (css_file.available())
		html_file.print(String(css_file.read()));
}

void MakeHead(File html_file)
{
	html_file.print("<head>");
	html_file.print("<style>");
	GenerateChart(html_file);
	html_file.print("</style>");
	html_file.print("</head>");
}

void MakeBody(File html_file)
{
	html_file.print("<body>");
	html_file.print("<table class='graph'>");
	html_file.print("<h1 style=\"margin-left: 10px; font-family: sans-serif; font-size: 25px; font-weight: bold;\">");
	html_file.print(display_date);
	html_file.print(".");
	html_file.print("</h1>");
	html_file.print("<div class='chart-wrap vertical'><div class='grid'>");
	GenerateChart(html_file);
	html_file.print("</div>");
	html_file.print("<div class=\"footr\">");
	html_file.print("<button onclick=\"location.href='/prev'\" class='button button1' style=\"transform: translateX(20px);\">Elozo</button>");
	html_file.print("<button onclick=\"location.href='/'\" class='button button1' style=\"transform: translateX(70px);\">Mai nap</button>");
	html_file.print("<button onclick=\"location.href='/next'\" class='button button1' style=\"transform: translateX(120px);\">Kovetkezo</button>");
	html_file.print("<button onclick=\"location.href='/data'\" class='button2' style=\"transform: translateX(550px);\">Data</button>");
	html_file.print("</div></div></div>");
	html_file.print("</body>");
}

void MakePage()
{
	File html_file = SPIFFS.open("/index.html");
	if (!html_file) return;

	Serial.println("Making HTML page.");

	// Making the HTML page functionally.
	html_file.print("<!DOCTYPE html>");
	MakeHead(html_file);
	MakeBody(html_file);

	html_file.close();
}

void WriteDataToCSV(int min, int max, int avg, File data_file)
{
	if (!data_file)
		return;

	int year = ds_clock.getYear(), month = ds_clock.getMonth(clock_props.century), day = ds_clock.getDate(), hour = ds_clock.getHour(clock_props.format_12, clock_props.pm), minute = ds_clock.getMinute();
	char data_log[33];

	sprintf(data_log, "%04d.%02d.%02d. %02d:%02d,%04d,%04d,%04d",
			2000 + year,
			month,
			day,
			hour,
			minute,
			min,
			max,
			avg);

	if (!data_file.println(data_log))
		Serial.println("Failed writting data file.");

	Serial.println(data_log);
	data_file.close();
}

void SetChartData()
{
	struct TemporaryData
	{
		String ptr;
		float min = AMPERAGE_MAX, max, avg;
		int time, cnt = 1;
		File file = SPIFFS.open(GetDataFileName());
	} tmp_data;

	if (!tmp_data.file)
		return;

	memset(data_pack, 0, sizeof(data_pack));

	while (tmp_data.file.available())
	{
		char fileRead = tmp_data.file.read();

		if (fileRead == '\n')
		{
			// * 0020.10.08. 13:07,0000,0000,0000

			if (tmp_data.time == tmp_data.ptr.substring(12, 14).toInt())
			{
				tmp_data.min = tmp_data.ptr.substring(18, 22).toInt() < tmp_data.min ? tmp_data.ptr.substring(18, 22).toInt() : tmp_data.min;
				tmp_data.max = tmp_data.ptr.substring(23, 27).toInt() > tmp_data.max ? tmp_data.ptr.substring(23, 27).toInt() : tmp_data.max;
				tmp_data.avg = round(tmp_data.ptr.substring(28, 32).toInt() / tmp_data.cnt);
				tmp_data.cnt++;

				data_pack[tmp_data.ptr.substring(12, 14).toInt()].min = tmp_data.min;
				data_pack[tmp_data.ptr.substring(12, 14).toInt()].max = tmp_data.max;
				data_pack[tmp_data.ptr.substring(12, 14).toInt()].avg = tmp_data.avg;
			}
			else
				tmp_data.time = tmp_data.ptr.substring(12, 14).toInt();

			tmp_data.ptr = "";
		}
		else
			tmp_data.ptr += fileRead;
	}
}

CollectedData collected_data;
Sensor sensors[3];

void GetChartData()
{
	DateTime now;

	if (collected_data.time == now.minute())
		return;

	collected_data.time = now.minute();

	for (size_t i = 0; i < *(&sensors + 1) - sensors; i++)
	{
		sensors[i].irms = random(1, 350); // * sensors[i].emon.calcIrms(1480);

		collected_data.min = sensors[i].irms < collected_data.min ? sensors[i].irms : collected_data.min;
		collected_data.max = sensors[i].irms > collected_data.max ? sensors[i].irms : collected_data.max;
		collected_data.sum += sensors[i].irms;
	}

	if (collected_data.cnt == 5)
	{
		WriteDataToCSV(collected_data.min, collected_data.max, collected_data.sum / collected_data.cnt, SPIFFS.open(GetDataFileName(), FILE_APPEND));

		collected_data.min = AMPERAGE_MAX;
		collected_data.max = 0;
		collected_data.sum = 0;
		collected_data.cnt = 0;
	}

	collected_data.cnt++;
	Serial.println("Got minute data.");
}

void InitializeSensors()
{
	sensors[0].emon.current(32, 10);
	sensors[1].emon.current(33, 10);
	sensors[2].emon.current(35, 10);
}

String GetDataFileName()
{
	String file_name = "/";
	file_name += display_date;
	file_name += ".csv";

	return file_name;
}

WebServer server;

void StreamFile(const char *path, String mimeType)
{
	// Streaming the generated html file, if it exists.
	if (SPIFFS.exists(path))
	{
		File file = SPIFFS.open(path, "r");
		server.streamFile(file, mimeType);
		file.close();
	}
	else
		HandleNotFound();
}

// On connect, the device would recieve the HTML data.
void HandleOnConnect()
{
	Serial.println("Connection handling called.");

	SetChartData();							// Aligns the data for the HTML page.
	MakePage();								// Making HTML page.
	StreamFile("/index.html", "text/html"); // Streaming HTML.
}

void GetNextDay()
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

void GetPreviousDay()
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

void HandleToday()
{
	Serial.println("Handling current day record.");
	display_date = GetCurrentDate();
	HandleOnConnect();
}

void HandleNextDay()
{
	Serial.println("Handling next day record.");
	GetNextDay();
	HandleOnConnect();
}

void HandlePrevDay()
{
	Serial.println("Handling previous day record.");
	GetPreviousDay();
	HandleOnConnect();
}

// If the server was not found, the device would return 404.
void HandleNotFound()
{
	Serial.println("Error: 404 (Not found)");
	server.send(404, "text/plain", "Not found");
}

void HandleDump()
{
	Serial.println("CSV Dump");
	StreamFile(GetDataFileName().c_str(), "text/plain");
}

void InitializeServer()
{
	server.on("/", HandleToday);
	server.on("/next", HandleNextDay);
	server.on("/prev", HandlePrevDay);
	server.on("/data", HandleDump);

	server.onNotFound(HandleNotFound);
	server.begin();
}
