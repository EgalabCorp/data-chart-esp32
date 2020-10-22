#ifndef __DATACHART_H__
#define __DATACHART_H__

#pragma once

#include <Arduino.h>

#include <DS3231.h>
#include <EmonLib.h>
#include <SPIFFS.h>
#include <WebServer.h>

void DataInit();
void DataProcess();

struct CollectedData
{
	int min, max, time, sum, cnt = 1;
};

struct ParsableData
{
	int min, max, avg;
};

struct Sensor
{
	EnergyMonitor emon;
	double irms;
};

struct ClockProperties
{
	bool century, format_12, pm;
};

void InitializeTime();
int GetCurrentDate();

void GenerateChart(File html_file);
void GenerateStyle(File html_file);

void MakeHead(File html_file);
void MakeBody(File html_file);
void MakePage(File html_file);

void WriteDataToCSV(int min, int max, int avg, File data_file);

void SetChartData();
void GetChartData();

void InitializeSensors();

String GetDataFileName();

void StreamFile(const char *path, String mimeType);
void HandleOnConnect();
void GetNextDay();
void GetPreviousDay();
void HandleToday();
void HandlePrevDay();
void HandleNextDay();
void HandleNotFound();
void InitializeServer();

#endif // __DATACHART_H__