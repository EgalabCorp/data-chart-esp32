#include "DataChart.h"

RTC_DS3231 rtc;

void InitializeTime()
{
	if (!rtc.begin())
	{
		Serial.println("Couldn't find RTC.");
		Serial.flush();
		abort();
	}

	if (rtc.lostPower())
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

	// Nincs szükség a 32K pin-re.
	rtc.disable32K();
}

String getCurrentDate()
{
	DateTime now = rtc.now();
	char buf[] = "MM-DD-YYYY";
	return now.toString(buf);
}