#ifndef __DATACHART_H__
#define __DATACHART_H__

#pragma once

#include <Arduino.h>

#include <DS3231.h>
#include <EmonLib.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <Wire.h>

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

void GenerateChart(File html_file);
void GenerateStyle(File html_file);

void MakeHead(File html_file);
void MakeBody(File html_file);
void MakePage(File html_file);

void InitializeTime();
int GetCurrentDate();

void GetChartData();
void SetChartData();

String GetDataFileName();

void WriteDataToCSV(int min, int max, int avg);

void InitializeSensors();

void StreamFile(const char *path, String mimeType);
void GetNextDay();
void GetPreviousDay();
void HandleNotFound();
void HandleToday();
void HandlePrevDay();
void HandleNextDay();
void InitializeServer();

void DataInit();
void DataProcess();

#endif // __DATACHART_H__