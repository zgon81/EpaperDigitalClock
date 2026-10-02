#include <Arduino.h>
#include "Epaper213.h"

Epaper213 epd;

namespace {
	constexpr uint16_t CLOCK_X = 5, CLOCK_Y = 0;
	constexpr uint16_t DAY_X = 5, DAY_Y = 90;
	uint32_t last = 0;
	bool ready = false;
}

void setup() {
	Serial.begin(115200);
	delay(500);
	if (!epd.begin()) return;
	epd.clear();
	epd.drawText(CLOCK_X, CLOCK_Y, "23:45", EpaperFont::ClockBold);
	epd.drawText(DAY_X, DAY_Y, "czwartek", EpaperFont::LargeBold);
	if (!epd.fullRefresh()) return;
	ready = true;
	last = millis();
}

void loop() {
	if (!ready || millis() - last < 1000) return;
	last = millis();
}