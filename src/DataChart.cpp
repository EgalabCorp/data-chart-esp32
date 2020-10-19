#include "DataChart.h"

RTC_DS3231 rtc;

void InitializeTime()
{
	rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

	if (!rtc.begin())
	{
		Serial.println("Couldn't find RTC");
		Serial.flush();
		abort();
	}

	rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

int getCurrentDate()
{
	DateTime now = rtc.now();
	char buf[] = "YYYYMMDD";
	String s = now.toString(buf);
	return s.toInt();
}