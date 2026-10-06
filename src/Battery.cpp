#include "Battery.h"

struct BatteryPoint
{
	float voltage;
	int percent;
};

const BatteryPoint batteryTable[] =
{
	{4.20, 100},
	{4.15, 95},
	{4.10, 90},
	{4.05, 85},
	{4.00, 80},
	{3.95, 75},
	{3.90, 70},
	{3.85, 60},
	{3.80, 50},
	{3.75, 40},
	{3.70, 30},
	{3.65, 20},
	{3.60, 10},
	{3.50, 5},
	{3.30, 0}
};

const int BATTERY_TABLE_SIZE =
	sizeof(batteryTable) / sizeof(batteryTable[0]);


int batteryPercentFromVoltage(float voltage)
{
	if (voltage >= batteryTable[0].voltage)
		return 100;

	if (voltage <= batteryTable[BATTERY_TABLE_SIZE - 1].voltage)
		return 0;

	for (int i = 0; i < BATTERY_TABLE_SIZE - 1; i++)
	{
		float vHigh = batteryTable[i].voltage;
		float vLow = batteryTable[i + 1].voltage;

		if (voltage <= vHigh && voltage >= vLow)
		{
			int pHigh = batteryTable[i].percent;
			int pLow = batteryTable[i + 1].percent;

			float ratio =
				(voltage - vLow) / (vHigh - vLow);

			return pLow + (int)((pHigh - pLow) * ratio);
		}
	}

	return 0;
}