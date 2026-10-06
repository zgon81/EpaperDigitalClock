#include "Clock.h"
#include "ClockConfig.h"
#include "ClockLogic.h"
#include "Epaper213.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

namespace {
	constexpr uint32_t STATE_MAGIC = 0x434C4B02;
	struct ClockState {
		uint32_t magic;
		time_t nextNtpSync;
		time_t lastFullRefresh;
		time_t testTime;
		uint16_t partialRefreshCount;
		int previousHour;
		int previousMinute;
		int previousWeekday;
		bool displayValid;
		bool displayedTimeValid;
	};
	RTC_DATA_ATTR ClockState state = {};
	Epaper213 epd;
	bool panelReady = false;
	bool deepSleepEnabled = true;
	uint32_t nextCycleMs = 0;
	uint32_t lastHeartbeatMs = 0;

	time_t clockNow() {
		return Config::CLOCK_TEST_MODE ? state.testTime : time(nullptr);
	}

	void disableWifi() {
		WiFi.disconnect(true);
		WiFi.mode(WIFI_OFF);
		Serial.println("WiFi OFF");
	}

	bool syncTimeFromNtp() {
		if (Config::CLOCK_TEST_MODE) {
			Serial.println("NTP sync simulated");
			return true;
		}
		if (!Config::WIFI_SSID[0]) {
			Serial.println("WiFi SSID not configured");
			return false;
		}
		Serial.println("Connecting WiFi...");
		WiFi.persistent(false);
		WiFi.mode(WIFI_STA);
		WiFi.setAutoReconnect(false);
		WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
		uint32_t start = millis();
		while (WiFi.status() != WL_CONNECTED && millis() - start < Config::WIFI_TIMEOUT_MS)
			delay(20);
		if (WiFi.status() != WL_CONNECTED) {
			Serial.printf("WiFi connection timeout; status=%d\n", static_cast<int>(WiFi.status()));
			disableWifi();
			return false;
		}
		Serial.println("WiFi connected\nSynchronizing NTP...");
		Serial.print("IP: ");
		Serial.println(WiFi.localIP());
		sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
		sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
		configTzTime(Config::TIME_ZONE, Config::NTP_SERVER_1, Config::NTP_SERVER_2);
		start = millis();
		bool synced = false;
		while (millis() - start < Config::NTP_TIMEOUT_MS) {
			if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
				synced = ClockLogic::isTimeValid(time(nullptr));
				break;
			}
			delay(20);
		}
		esp_sntp_stop();
		disableWifi();
		Serial.println(synced ? "NTP sync OK" : "NTP sync timeout / invalid time");
		return synced;
	}

	void formatClock(int hour, int minute, bool valid, char (&text)[6]) {
		if (valid) snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
		else memcpy(text, "  :  ", sizeof(text));
	}

	void drawClockToFramebuffer(int hour, int minute, bool valid, int weekday) {
		epd.clear();
		char text[6];
		formatClock(hour, minute, valid, text);
		epd.drawText(Config::CLOCK_X, Config::CLOCK_Y, text, EpaperFont::ClockBold);
		if (valid) {
			const char* day = ClockLogic::weekdayName(weekday);
			const uint16_t width = Epaper213::textWidth(day, EpaperFont::MediumBold);
			epd.drawText((Epaper213::WIDTH - width) / 2, Config::WEEKDAY_Y,
				day, EpaperFont::LargeBold);
		}
		if (!valid) {
			for (uint8_t cell = 0; cell < 5; ++cell) {
				if (cell == 2) continue;
				epd.fillRect(Config::CLOCK_X + cell * Config::DIGIT_WIDTH + 12,
					Config::CLOCK_Y + 45, 24, 6, false);
			}
		}
	}

	bool performFullRefresh(time_t now) {
		Serial.println("Display: full refresh");
		if (!epd.fullRefresh()) return false;
		state.partialRefreshCount = 0;
		state.lastFullRefresh = now;
		return true;
	}

	bool performPartialRefresh(uint8_t first, uint8_t last) {
		Serial.println("Display: partial refresh");
		if (!epd.partialRefresh(Config::CLOCK_X + first * Config::DIGIT_WIDTH,
			Config::CLOCK_Y, (last - first + 1) * Config::DIGIT_WIDTH, Config::DIGIT_HEIGHT))
			return false;
		++state.partialRefreshCount;
		Serial.printf("Partial count: %u\n", state.partialRefreshCount);
		return true;
	}

	bool updateClockDisplay(time_t now, bool forceFullRefresh) {
		const bool valid = ClockLogic::isTimeValid(now);
		tm currentTime = {};
		if (valid) localtime_r(&now, &currentTime);
		const int hour = valid ? currentTime.tm_hour : -1;
		const int minute = valid ? currentTime.tm_min : -1;
		const int weekday = valid ? currentTime.tm_wday : -1;
		bool full = ClockLogic::needFullRefresh(forceFullRefresh, state.displayValid,
			valid, hour, state.previousHour, now, state.lastFullRefresh,
			state.partialRefreshCount, Config::MAX_PARTIAL_REFRESH_COUNT) ||
			valid != state.displayedTimeValid || weekday != state.previousWeekday;
		const bool changed = hour != state.previousHour || minute != state.previousMinute;
		if (valid) Serial.printf("Time: %02d:%02d\n", hour, minute);
		else Serial.println("Time: --:-- (invalid)");
		if (!full && !changed) {
			drawClockToFramebuffer(hour, minute, valid, weekday);
			return true;
		}
		if (!panelReady) {
			panelReady = epd.begin();
			if (!panelReady) {
				state.displayValid = false;
				return false;
			}
		}
		if (!full) {
			drawClockToFramebuffer(state.previousHour, state.previousMinute,
				state.displayedTimeValid, state.previousWeekday);
			if (!epd.restoreBaseImage()) full = true;
		}
		drawClockToFramebuffer(hour, minute, valid, weekday);
		bool ok = true;
		if (full) ok = performFullRefresh(now);
		else {
			char previous[6], current[6];
			formatClock(state.previousHour, state.previousMinute, true, previous);
			formatClock(hour, minute, true, current);
			const char* names[] = {"hour tens", "hour ones", "colon", "minute tens", "minute ones"};
			for (uint8_t cell = 0; cell < 5 && ok; ++cell) {
				if (previous[cell] == current[cell]) continue;
				const uint8_t first = cell;
				Serial.printf("Changed digit: %s\n", names[cell]);
				while (cell + 1 < 5 && previous[cell + 1] != current[cell + 1]) {
					++cell;
					Serial.printf("Changed digit: %s\n", names[cell]);
				}
				ok = performPartialRefresh(first, cell);
			}
		}
		state.displayValid = ok;
		if (ok) {
			state.previousHour = hour;
			state.previousMinute = minute;
			state.previousWeekday = weekday;
			state.displayedTimeValid = valid;
		} else Serial.println("Display error; full refresh required next wake");
		return ok;
	}

	void sleepUntilNextMinute() {
		if (panelReady) {
			epd.sleep();
			panelReady = false;
		}
		// Stop Serial before measuring the remaining minute to avoid flush drift.
		Serial.flush();
		timeval tv;
		gettimeofday(&tv, nullptr);
		uint64_t sleepUs = Config::CLOCK_TEST_MODE ? Config::TEST_SLEEP_US :
			ClockLogic::sleepToMinute(tv.tv_sec, tv.tv_usec);
		if (!deepSleepEnabled) {
			// Monotonic deadline, independent of NTP wall-clock corrections.
			nextCycleMs = millis() + static_cast<uint32_t>((sleepUs + 999ULL) / 1000ULL);
			Serial.println("Deep sleep disabled; waiting in loop");
			return;
		}
		Serial.printf("Entering deep sleep for %llu us\n", static_cast<unsigned long long>(sleepUs));
		Serial.flush();
		if (!Config::CLOCK_TEST_MODE) {
			gettimeofday(&tv, nullptr);
			sleepUs = ClockLogic::sleepToMinute(tv.tv_sec, tv.tv_usec);
		}
		esp_sleep_enable_timer_wakeup(sleepUs);
		esp_deep_sleep_start();
	}
	void runClockCycle(bool firstCycle);
}

void setupClock(bool enableDeepSleep) {
	deepSleepEnabled = enableDeepSleep;
	Serial.begin(115200);
	if (!deepSleepEnabled) {
		const uint32_t start = millis();
		while (!Serial && millis() - start < Config::SERIAL_WAIT_MS) delay(10);
	}
	const bool timerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER &&
		state.magic == STATE_MAGIC;
	Serial.println(timerWake ? "Boot: timer wakeup" : "Boot: cold start / reset");
	if (!timerWake) {
		state = {};
		state.magic = STATE_MAGIC;
		state.previousHour = -1;
		state.previousMinute = -1;
		state.previousWeekday = -1;
		state.testTime = 1767265140; // 2026-01-01 11:59 UTC: test hour rollover.
	} else if (Config::CLOCK_TEST_MODE) state.testTime += 60;
	setenv("TZ", Config::TIME_ZONE, 1);
	tzset();
	lastHeartbeatMs = millis();
	runClockCycle(!timerWake);
}

namespace {
	void runClockCycle(bool firstCycle) {
		time_t now = clockNow();
		bool synced = false;
		if (firstCycle || ClockLogic::shouldSyncNtp(now, state.nextNtpSync)) {
			Serial.println("NTP sync required");
			synced = syncTimeFromNtp();
			now = clockNow();
			// Schedule attempts as well as successes: no WiFi storm when offline.
			state.nextNtpSync = now + ClockLogic::ntpAttemptInterval(Config::CLOCK_TEST_MODE,
				deepSleepEnabled, synced, ClockLogic::isTimeValid(now), Config::NTP_INTERVAL_SECONDS,
				Config::TEST_NTP_INTERVAL_SECONDS, Config::DEBUG_NTP_RETRY_SECONDS);
			Serial.printf("Next NTP sync: %lld\n", static_cast<long long>(state.nextNtpSync));
		} else Serial.println("NTP sync not required");
		tm corrected = {};
		const bool valid = ClockLogic::isTimeValid(now);
		Serial.printf("System time: %lld; valid=%s\n", static_cast<long long>(now), valid ? "yes" : "no");
		if (valid) localtime_r(&now, &corrected);
		const bool visibleCorrection = synced && (valid != state.displayedTimeValid ||
			corrected.tm_hour != state.previousHour || corrected.tm_min != state.previousMinute);
		if (synced) Serial.println("Display update after NTP");
		bool ok = updateClockDisplay(now, firstCycle || visibleCorrection);
		// Bounded catch-up if processing crossed a minute boundary.
		for (uint8_t attempt = 0; ok && !Config::CLOCK_TEST_MODE && attempt < 2; ++attempt) {
			now = clockNow();
			if (!ClockLogic::isTimeValid(now)) break;
			tm latest;
			localtime_r(&now, &latest);
			if (latest.tm_hour == state.previousHour && latest.tm_min == state.previousMinute &&
				latest.tm_wday == state.previousWeekday) break;
			ok = updateClockDisplay(now, false);
		}
		sleepUntilNextMinute();
	}
}

void loopClock() {
	if (deepSleepEnabled) return;
	const uint32_t nowMs = millis();
	if (nowMs - lastHeartbeatMs >= Config::DEBUG_HEARTBEAT_MS) {
		lastHeartbeatMs = nowMs;
		Serial.printf("Clock alive; system time=%lld; next NTP=%lld\n",
			static_cast<long long>(clockNow()), static_cast<long long>(state.nextNtpSync));
	}
	if (ClockLogic::deadlineReached(nowMs, nextCycleMs) ||
		ClockLogic::shouldSyncNtp(clockNow(), state.nextNtpSync)) {
		if (Config::CLOCK_TEST_MODE) state.testTime += 60;
		runClockCycle(false);
	}
	delay(10); // Yield while awake; never busy-spin until the next minute.
}