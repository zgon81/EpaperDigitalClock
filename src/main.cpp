#include <Arduino.h>
#include "Clock.h"
#include "ClockConfig.h"

void setup() {
	setupClock(Config::DEEP_SLEEP_ENABLED);
}

void loop() {
	loopClock();
}