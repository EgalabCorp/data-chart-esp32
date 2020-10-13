#include <DS3231.h>
#include <EmonLib.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

// Ezzel könnyebb lesz majd állítani a maximum áramerősséget.
#define AMPERAGE_MAX 1200

// The date responsible for the file naming which is holding the data.
static int displayDate;

// Clock hardware module.
DS3231 Clock;

// Webserver setup for networking.
WebServer server(80);

struct TimeClock
{
	bool century, format12, pm;
} timeClock;

struct Sensor
{
	EnergyMonitor emon;
	double Irms;
} sensors[3];

struct CollectedData
{
	int min, max, sum, cnt, time;
} collectedData;

struct ProcessedData
{
	int min, max, avg;
} processedData[24];

int getCurrentDate()
{
	char getDate[9];
	sprintf(getDate, "%04d%02d%02d", 2000 + Clock.getYear(), Clock.getMonth(timeClock.century), Clock.getDate());

	return String(getDate).toInt();
}

// On connect, the device would recieve the HTML data.
void handleOnConnect()
{
	Serial.println("Connection handling called.");

	makePage();								// Making HTML page.
	streamFile("/index.html", "text/html"); // Streaming HTML.
}

// If the server was not found, the device would return 404.
void handleNotFound()
{
	Serial.println("Error: 404 (Not found)");
	server.send(404, "text/plain", "Not found");
}

void streamFile(const char *path, String mimeType)
{
	// Streaming the generated html file, if it exists.
	if (SPIFFS.exists(path))
	{
		File file = SPIFFS.open(path, "r");
		server.streamFile(file, mimeType);
		file.close();
	}
	else
		handleNotFound();
}

void handleToday()
{
	Serial.println("Handling current day record.");
	displayDate = getCurrentDate();
	handleOnConnect();
}

void handlePrevDay()
{
	Serial.println("Handling previous day record.");
	getPreviousDate();
	handleOnConnect();
}

void handleNextDay()
{
	Serial.println("Handling next day record.");
	getNextDate();
	handleOnConnect();
}

void networkInit()
{
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

	Serial.println(WiFi.localIP());
	server.on("/", handleToday);

	server.on("/next", handleNextDay);
	server.on("/prev", handlePrevDay);

	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("Server started.");
}

void timeInit()
{
	Serial.println("Initializing time...");

	for (int i = 0; i < 5; i++)
	{
		delay(1000);
		Serial.print(Clock.getYear(), DEC);
		Serial.print(".");
		Serial.print(Clock.getMonth(timeClock.century), DEC);
		Serial.print(".");
		Serial.print(Clock.getDate(), DEC);
		Serial.print(". ");
		Serial.print(Clock.getHour(timeClock.format12, timeClock.pm), DEC);
		Serial.print(":");
		Serial.print(Clock.getMinute(), DEC);
		Serial.print(":");
		Serial.println(Clock.getSecond(), DEC);
	}

	displayDate = getCurrentDate();
}

void setDisplayDate(int date)
{
	File file = SPIFFS.open(getDataFileName(), FILE_READ);
	if (file)
	{
		displayDate = date;
		Serial.print("File found:");
		Serial.println(getDataFileName());
	}
}

void getPreviousDate()
{
	for (size_t i = displayDate - 1; i > 20201008; i--)
		setDisplayDate(i);
}

void getNextDate()
{
	for (size_t i = displayDate + 1; i < getCurrentDate(); i++)
		setDisplayDate(i);
}

/*
dataProcess()

- szedjuk szet az adatgyujtest es feldologzast

Adatgyujtes:
- percenkent lefut 
- vesz adatot
- db szam novel
- sum = sum + vett adat – ossze szenzorra
- megnezi, hogy eltelt-e 5 perc (konstansbol jojjon)
- ha eltelt, akkor kiirja a min/max/avg-t a megfelelo csv-be (=datayyyymmdd.csv)
- "nullazza" a valtozokat


Feldologzast
- kell egy 24 elemu tomb, amibe a megfelelo kapott ertekeket rakjuk (maybe struct...)
- a parameterbol jovo datumnak megfelelo csv megnyitasa
- vegig olvasas, soronkent
- sor feldolgozas, min, max avg kinyeres, es a tombbe bele


Kiiras
- ha megvan a tomb, akkor irjuk ki


*/

void getData()
{
	if (collectedData.time == Clock.getMinute())
		return;

	collectedData.time = Clock.getMinute();

	for (size_t i = 0; i < *(&sensors + 1) - sensors; i++)
	{
		sensors[i].Irms = random(350); // * sensors[i].emon.calcIrms(1480);

		collectedData.min = sensors[i].Irms < collectedData.min ? sensors[i].Irms : collectedData.min;
		collectedData.max = sensors[i].Irms > collectedData.max ? sensors[i].Irms : collectedData.max;
		collectedData.sum += sensors[i].Irms;
	}

	if (collectedData.cnt == 5)
	{
		writeDataToCSV(collectedData.min, collectedData.max, collectedData.sum / collectedData.cnt);

		collectedData.min = AMPERAGE_MAX;
		collectedData.max = 0;
		collectedData.sum = 0;
		collectedData.cnt = 0;
	}

	collectedData.cnt++;
}

void setData()
{
	getData();

	File file = SPIFFS.open(getDataFileName());
	if (!file)
		Serial.println("A file-t nem sikerült megnyitni adatfeldolgozásra.");

	struct HourlyData
	{
		String ptr = "";
		int min = AMPERAGE_MAX;
		int max, avg, time, ind, cnt = 1;
	} tmp;

	Serial.print("Index = ");
	Serial.println(tmp.ind);

	while (file.available())
	{
		char fileRead = file.read();

		if (fileRead == '\n')
		{
			tmp.min = tmp.ptr.substring(18, 22).toInt() < tmp.min ? tmp.ptr.substring(18, 22).toInt() : tmp.min;
			tmp.max = tmp.ptr.substring(23, 27).toInt() > tmp.max ? tmp.ptr.substring(23, 27).toInt() : tmp.max;
			tmp.avg = round(tmp.ptr.substring(28, 32).toInt() / tmp.cnt);

			tmp.cnt++;
		}
		else
			tmp.ptr += fileRead;
	}

	Serial.println("The file has been processed.");

	if (tmp.time != Clock.getHour(timeClock.format12, timeClock.pm))
	{
		processedData[tmp.ind].min = tmp.min;
		processedData[tmp.ind].max = tmp.max;
		processedData[tmp.ind].avg = tmp.avg;

		tmp.ind = tmp.ind <= *(&processedData + 1) - processedData ? tmp.ind + 1 : tmp.ind;
		tmp.time = Clock.getMinute();
	}
}

String getDataFileName()
{
	String fileName = "/";
	fileName += displayDate;
	fileName += ".csv";

	return fileName;
}

void writeDataToCSV(int min, int max, int avg)
{
	struct DateTime
	{
		int year, month, date, hour, minute;
	} dateTime;

	// File holder for the main data file.
	File file = SPIFFS.open(getDataFileName(), FILE_APPEND);

	Serial.print("File name:");
	Serial.println(getDataFileName());

	if (!file)
	{
		Serial.println("Failed to open file for writing.");
		return;
	}

	char dataLog[33] = "";

	dateTime.year = Clock.getYear();
	dateTime.month = Clock.getMonth(timeClock.century);
	dateTime.date = Clock.getDate();
	dateTime.hour = Clock.getHour(timeClock.format12, timeClock.pm);
	dateTime.minute = Clock.getMinute();

	// Sor formázás
	sprintf(dataLog, "%04d.%02d.%02d. %02d:%02d,%04d,%04d,%04d", 2000 + dateTime.year, dateTime.month, dateTime.date, dateTime.hour, dateTime.minute, min, max, avg);

	if (!file.println(dataLog))
		Serial.println("Writing failed.");

	Serial.println(dataLog);
	file.close();
}

String fileToString(const char *path)
{
	File file = SPIFFS.open(path);
	if (!file)
	{
		Serial.println("Failed to open file. (File to String)");
		return "";
	}

	String output = "";
	while (file.available())
	{
		char fileRead = file.read();
		output += fileRead;
	}

	return output;
}

void generateChart(File html)
{
	struct DataRead
	{
		int min, max, avg;
	} dataRead;

	File file = SPIFFS.open(getDataFileName());
	if (!file)
	{
		Serial.println("Error loading data file.");
		return;
	}

	//0020.10.08. 13:07,0000,0000,0000
	String ptr = "";

	for (size_t i = 0; i < 24; i++)
	{
		char fileRead = file.read();

		if (fileRead == '\n')
		{
			dataRead.min = processedData[i].min;
			dataRead.max = processedData[i].max;
			dataRead.avg = processedData[i].avg;

			Serial.println("----------");
			Serial.println(dataRead.min);
			Serial.println(dataRead.max);
			Serial.println(dataRead.avg);

			html.print("<div class='bar' style='--bar-value:");
			html.print(round(dataRead.max / 12));
			html.print("%;' data-name='");
			html.print(ptr.substring(13, 14));
			html.print("h' title='");
			html.print(String(dataRead.max));
			html.print("'>");
			html.print("<div class='bar2' style='--bar-value:");
			html.print(round(dataRead.avg / dataRead.max * 100));
			html.print("%;' data-name='");
			html.print(ptr.substring(13, 14));
			html.print("h' title='");
			html.print(String(dataRead.avg));
			html.print("'>");
			html.print("<div class='bar3' style='--bar-value:");
			html.print(round(dataRead.min / dataRead.avg * 100));
			html.print("%;' data-name='");
			html.print(ptr.substring(13, 14));
			html.print("h' title='");
			html.print(String(dataRead.min));
			html.print("'>");
			html.print("</div></div></div>");

			ptr = "";
		}
		else
			ptr += fileRead;
	}
}

// Imports the external CSS from SPIFFS.
void makeHead(File file)
{
	file.print("<head>");
	file.print("<style>");
	file.print(fileToString("/style2.css"));
	file.print("</style>");
	file.print("</head>");
}

// Makes the chart for in the body.
void makeBody(File file)
{
	file.print("<body>");
	file.print("<table class='graph'>");
	file.print("<caption>");
	file.print("20");
	file.print(String(Clock.getYear()));
	file.print(". ");
	file.print(String(Clock.getMonth(timeClock.century)));
	file.print(". ");
	file.print(String(Clock.getDate()));
	file.print(".");
	file.print("</caption>");
	file.print("<div class='chart-wrap vertical'><div class='grid'>");
	generateChart(file);
	file.print("</br></br></br></br></br>");
	file.print("<table width='1000px' border=0>");
	file.print("<tr><td align=center width=33%><button onclick=\"location.href='/'\" class='button button1'>Mai nap</button></td>");
	file.print("<td width=33%><button onclick=\"location.href='/prev'\" class='button button1'>Elozo</button></td>");
	file.print("<td width=33%><button onclick=\"location.href='/next'\" class='button button1'>Kovetkezo</button></td>");
	file.print("</tr><tr><td align=center width=33%><button onclick=\"location.href='/data'\" class='button2'>data</button></td>");
	file.print("<td>&nbsp;</td><td>&nbsp;</td></tr>");
	file.print("</table>");
	file.print("</body>");
}

void makePage()
{
	Serial.println("Attempting to make HTML page.");

	File file = SPIFFS.open("/index.html", FILE_WRITE);
	if (!file)
	{
		Serial.println("Error making HTML page. (SPIFFS).");
		return;
	}

	// Making the HTML page functionally.
	file.print("<!DOCTYPE html>");
	makeHead(file);
	makeBody(file);

	file.close();
}

// Init
void setup()
{
	Serial.begin(115200);

	if (!SPIFFS.begin())
		Serial.println("Failed SPIFFS inizialization.");

	Wire.begin();

	networkInit();
	timeInit();

	sensors[0].emon.current(32, 10);
	sensors[1].emon.current(33, 10);
	sensors[2].emon.current(35, 10);
}

// Process
void loop()
{
	setData();
	server.handleClient();
}
