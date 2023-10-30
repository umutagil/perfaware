#include "profiler.h"

#include <stdio.h>

inline f64 ToMegabyte(const u64 bytes)
{
	return static_cast<f64>(bytes) / (1024.0 * 1024.0);
}

inline f64 ToGigabyte(const u64 bytes)
{
	return static_cast<f64>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

#if PROFILER
u32 Profiler::GetNewIndex()
{
	return ++indexCounter;
}

void PrintBlockInfo(const ProfilerBlockInfo& info, const u64 totalCpuElapsed, const u64 cpuFreq)
{
	const f64 percentageExclusive = 100.0 * static_cast<f64>(info.elapsedTimeExclusive) / static_cast<f64>(totalCpuElapsed);
	printf("  %s[%llu]: %llu (%.2f%%", info.name, info.hitCount, info.elapsedTimeExclusive, percentageExclusive);

	if (info.elapsedTimeInclusive != info.elapsedTimeExclusive) {
		const f64 percentageInclusive = 100.0 * static_cast<f64>(info.elapsedTimeInclusive) / static_cast<f64>(totalCpuElapsed);
		printf(", %.2f%% w/children", percentageInclusive);
	}

	printf(")");

	if (info.processedByteCount) {
		const f64 durationSec = static_cast<f64>(info.elapsedTimeInclusive) / static_cast<f64>(cpuFreq);
		const f64 gigabytesPerSec = ToGigabyte(info.processedByteCount) / durationSec;
		const f64 processedMegabytes = ToMegabyte(info.processedByteCount);
		printf("  %.3fmb at %.2fgb/s ", processedMegabytes, gigabytesPerSec);
	}

	printf("\n");
}

#else
#define PrintBlockInfo(...)
#endif

u64 Profiler::beginCpuTime;
u64 Profiler::endCpuTime;
u32 Profiler::indexCounter;
u32 Profiler::parentBlockIndex;
ProfilerBlockInfo Profiler::blocks[4096];

void Profiler::Begin()
{
	beginCpuTime = TimerFunc();
}

void Profiler::End()
{
	endCpuTime = TimerFunc();
}

void Profiler::PrintBlocks()
{
	if (endCpuTime == 0) {
		End();
	}

	const u64 totalCpuElapsed = endCpuTime - beginCpuTime;
	const u64 cpuFreq = CpuFrequencyFunc();
	const f64 totalCpuTime = static_cast<f64>(totalCpuElapsed) * 1000 / static_cast<f64>(cpuFreq);
	printf("Total time: %.4fms (CPU freq %llu)\n", totalCpuTime, cpuFreq);

	for (size_t i = 1; i <= indexCounter; ++i) {
		PrintBlockInfo(blocks[i], totalCpuElapsed, cpuFreq);
	}
}