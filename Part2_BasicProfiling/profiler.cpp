#include "profiler.h"

#include <stdio.h>

u64 Profiler::beginCpuTime;
u64 Profiler::endCpuTime;
u32 Profiler::indexCounter;
u32 Profiler::parentBlockIndex;
ProfilerBlockInfo Profiler::blocks[4096];

void Profiler::Begin()
{
	beginCpuTime = ReadCPUTimer();
}

void Profiler::End()
{
	endCpuTime = ReadCPUTimer();
}

u32 Profiler::GetNewIndex()
{
	return ++indexCounter;
}

void Profiler::AddBlock(const char* name, const u64 elapsedTime, const size_t idx, const size_t parentIdx)
{
	ProfilerBlockInfo& block(blocks[idx]);
	block.name = name;
	block.elapsedTime += elapsedTime;
	++block.hitCount;

	ProfilerBlockInfo& parentBlock(blocks[parentIdx]);
	parentBlock.elapsedTimeChildren += elapsedTime;
}

void PrintBlockInfo(const ProfilerBlockInfo& info, const u64 totalCpuElapsed)
{
	const u64 elapsedTimeNoChild = info.elapsedTime - info.elapsedTimeChildren;
	const f64 percentage = 100.0 * static_cast<f64>(elapsedTimeNoChild) / static_cast<f64>(totalCpuElapsed);
	printf("  %s[%llu]: %llu (%.2f%%", info.name, info.hitCount, elapsedTimeNoChild, percentage);

	if (info.elapsedTimeChildren) {
		const f64 percentageChildren = 100.0 * static_cast<f64>(info.elapsedTime) / static_cast<f64>(totalCpuElapsed);
		printf(", %.2f%% w/children", percentageChildren);
	}

	printf(")\n");
}

void Profiler::PrintBlocks()
{
	if (endCpuTime == 0) {
		End();
	}

	const u64 totalCpuElapsed = endCpuTime - beginCpuTime;
	const u64 cpuFreq = GetEstimatedCPUFrequency();
	const f64 totalCpuTime = static_cast<f64>(totalCpuElapsed) * 1000 / static_cast<f64>(cpuFreq);
	printf("Total time: %.4fms (CPU freq %llu)\n", totalCpuTime, cpuFreq);

	for (size_t i = 1; i <= indexCounter; ++i) {
		PrintBlockInfo(blocks[i], totalCpuElapsed);
	}
}


