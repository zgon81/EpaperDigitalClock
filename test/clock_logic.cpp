#include "../src/ClockLogic.h"
#include "../src/ClockConfig.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void checkLocal(time_t utc, int hour, int minute, int dst) {
	tm local = {};
#ifdef _WIN32
	assert(localtime_s(&local, &utc) == 0);
#else
	assert(localtime_r(&utc, &local));
#endif
	assert(local.tm_hour == hour && local.tm_min == minute && local.tm_isdst == dst);
}

int main() {
	using namespace ClockLogic;
	const char* const weekdays[] = {
		"niedziela", "poniedziałek", "wtorek", "środa", "czwartek", "piątek", "sobota"
	};
	for (int day = 0; day < 7; ++day) assert(strcmp(weekdayName(day), weekdays[day]) == 0);
	assert(strcmp(weekdayName(-1), "") == 0);
	assert(strcmp(weekdayName(7), "") == 0);
	assert(!isTimeValid(0));
	assert(!isTimeValid(MIN_VALID_TIME - 1));
	assert(isTimeValid(MIN_VALID_TIME));
	assert(shouldSyncNtp(100, 0));
	assert(!shouldSyncNtp(99, 100));
	assert(shouldSyncNtp(100, 100));
	assert(!shouldSyncNtp(1000, 1000 + Config::NTP_INTERVAL_SECONDS));
	assert(ntpAttemptInterval(false, false, false, false, 21600, 120, 60) == 60);
	assert(ntpAttemptInterval(false, true, false, false, 21600, 120, 60) == 21600);
	assert(ntpAttemptInterval(false, false, true, true, 21600, 120, 60) == 21600);
	assert(ntpAttemptInterval(false, false, false, true, 21600, 120, 60) == 21600);
	assert(ntpAttemptInterval(true, false, true, true, 21600, 120, 60) == 120);
	assert(!deadlineReached(99, 100));
	assert(deadlineReached(100, 100));
	assert(!deadlineReached(0xfffffff0U, 20));
	assert(deadlineReached(20, 20));
	assert(deadlineReached(10, 0xfffffff0U));
	assert(sleepToMinute(120, 0) == 60000000);
	assert(sleepToMinute(179, 999999) == 1);
	assert(sleepToMinute(123, 500000) == 56500000);
	assert(!needFullRefresh(false, true, true, 12, 12, 100, 0, 29, 30));
	assert(needFullRefresh(false, true, true, 12, 12, 100, 0, 30, 30));
	assert(needFullRefresh(false, true, true, 13, 12, 100, 0, 0, 30));
	assert(needFullRefresh(false, true, true, 12, 12, 3600, 0, 0, 30));
	assert(needFullRefresh(false, true, true, 12, 12, 99, 100, 0, 30));
	assert(needFullRefresh(false, false, true, 12, 12, 100, 0, 0, 30));
	assert(needFullRefresh(true, true, true, 12, 12, 100, 0, 0, 30));
#ifndef _WIN32
	setenv("TZ", Config::TIME_ZONE, 1);
	tzset();
	// 2026-03-29 00:59 / 01:00 UTC: 01:59 CET -> 03:00 CEST.
	checkLocal(1774745940, 1, 59, 0);
	checkLocal(1774746000, 3, 0, 1);
	// 2026-10-25 00:59 / 01:00 UTC: 02:59 CEST -> 02:00 CET.
	checkLocal(1792889940, 2, 59, 1);
	checkLocal(1792890000, 2, 0, 0);
	puts("Clock logic and CET/CEST tests passed");
#else
	puts("Clock logic tests passed (POSIX TZ tests require Linux/newlib)");
#endif
}