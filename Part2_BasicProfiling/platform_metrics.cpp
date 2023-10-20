#include "platform_metrics.h"

u64 GetEstimatedCPUFrequency()
{
	const u64 MILISECONDS_TO_WAIT = 100;
	const u64 OSFreq = GetOSTimerFreq();

	const u64 cpuStart = ReadCPUTimer();
	const u64 osStart = ReadOSTimer();
	u64 osEnd = 0;
	u64 osElapsed = 0;
	const u64 osWaitTime = OSFreq * MILISECONDS_TO_WAIT / 1000;
	while (osElapsed < osWaitTime) {
		osEnd = ReadOSTimer();
		osElapsed = osEnd - osStart;
	}

	const u64 cpuEnd = ReadCPUTimer();
	const u64 cpuElapsed = cpuEnd - cpuStart;
	u64 cpuFreq = 0;
	if (osElapsed) {
		cpuFreq = OSFreq * cpuElapsed / osElapsed;
	}

	return cpuFreq;
}
