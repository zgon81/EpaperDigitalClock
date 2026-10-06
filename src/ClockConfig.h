#pragma once
#include <stdint.h>

namespace Config {
	constexpr const char* WIFI_SSID = "bajabongo24";
	constexpr const char* WIFI_PASSWORD = "papudrak52";
	constexpr const char* TIME_ZONE = "CET-1CEST,M3.5.0/2,M10.5.0/3";
	constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
	constexpr const char* NTP_SERVER_2 = "time.google.com";
	constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
	constexpr uint32_t NTP_TIMEOUT_MS = 10000;
	constexpr uint32_t NTP_INTERVAL_SECONDS = 6 * 60 * 60;
	constexpr uint16_t MAX_PARTIAL_REFRESH_COUNT = 30;
	constexpr bool CLOCK_TEST_MODE = false;
	constexpr bool DEEP_SLEEP_ENABLED = true; // Enable for battery operation.
	constexpr uint32_t SERIAL_WAIT_MS = 5000;
	constexpr uint32_t DEBUG_NTP_RETRY_SECONDS = 60;
	constexpr uint32_t DEBUG_HEARTBEAT_MS = 10000;
	constexpr uint64_t TEST_SLEEP_US = 2000000ULL;
	constexpr uint32_t TEST_NTP_INTERVAL_SECONDS = 120;
	constexpr uint16_t CLOCK_X = 5;
	constexpr uint16_t CLOCK_Y = 0;
	constexpr uint16_t DIGIT_WIDTH = 48;
	constexpr uint16_t DIGIT_HEIGHT = 96;
	constexpr uint16_t WEEKDAY_Y = CLOCK_Y + DIGIT_HEIGHT;
}