#pragma once
#include <stdint.h>
#include <time.h>

namespace ClockLogic {
	inline const char* weekdayName(int weekday) {
		static const char* const names[] = {
			"niedziela", "poniedziałek", "wtorek", "środa", "czwartek", "piątek", "sobota"
		};
		return weekday >= 0 && weekday < 7 ? names[weekday] : "";
	}
	constexpr time_t MIN_VALID_TIME = 1704067200; // 2024-01-01 UTC
	inline bool isTimeValid(time_t now) { return now >= MIN_VALID_TIME; }
	inline bool shouldSyncNtp(time_t now, time_t next) { return next == 0 || now >= next; }
	inline uint32_t ntpAttemptInterval(bool testMode, bool deepSleep, bool synced,
		bool valid, uint32_t normalInterval, uint32_t testInterval, uint32_t debugRetry) {
		return testMode ? testInterval : (!deepSleep && !synced && !valid ? debugRetry : normalInterval);
	}
	inline bool deadlineReached(uint32_t now, uint32_t deadline) {
		return static_cast<int32_t>(now - deadline) >= 0;
	}
	inline uint64_t sleepToMinute(time_t seconds, long micros) {
		const uint64_t elapsed = static_cast<uint64_t>(seconds % 60) * 1000000ULL + micros;
		return 60000000ULL - elapsed;
	}
	inline bool needFullRefresh(bool force, bool base, bool valid, int hour,
		int previousHour, time_t now, time_t lastFull, uint16_t count, uint16_t limit) {
		return force || !base || (valid && hour != previousHour) || count >= limit ||
			now < lastFull || now - lastFull >= 3600;
	}
}