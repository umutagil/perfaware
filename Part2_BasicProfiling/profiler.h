#pragma once

#include <source_location>

#include "basedef.h"
#include "platform_metrics.h"

#define GET_LINE_NUM std::source_location::current().line()

#define NAME_CONCAT(A, B) A##B
#define FILE_NAME_LITERAL(name, index) static const char NAME_CONCAT(name, index)[] = __FILE__;
#define CREATE_BLOCK(blockName, varNamePostfix) FILE_NAME_LITERAL(file, varNamePostfix)\
							ProfileBlock<NAME_CONCAT(file, varNamePostfix), GET_LINE_NUM> NAME_CONCAT(block, varNamePostfix)(blockName);
#define PROFILE_BLOCK(name) CREATE_BLOCK(name, __COUNTER__)
#define PROFILE_BLOCK_FUNCTION PROFILE_BLOCK(__func__)

class Profiler;

struct ProfilerBlockInfo
{
	const char* name;
	u64 elapsedTime = 0;
	u64 elapsedTimeChildren = 0;
	u64 elapsedTimeRoot = 0;
	u64 hitCount = 0;
};

template <const char* N, const unsigned ID>
class ProfileBlock
{
public:

	ProfileBlock(const char* blockName)
		: blockName(blockName)
		, beginCpuTime(ReadCPUTimer())
		, parentBlockIndex(Profiler::parentBlockIndex)
		, elapsedTimeAtRoot(Profiler::blocks[blockIndex].elapsedTimeRoot)
	{
		if (blockIndex == 0) {
			blockIndex = Profiler::GetNewIndex();
		}

		Profiler::parentBlockIndex = blockIndex;
	}

	~ProfileBlock()
	{
		const u64 elapsedTime = ReadCPUTimer() - beginCpuTime;
		ProfilerBlockInfo& block(Profiler::blocks[blockIndex]);
		++block.hitCount;
		block.name = blockName;
		block.elapsedTime += elapsedTime;
		block.elapsedTimeRoot = elapsedTimeAtRoot + elapsedTime;

		Profiler::blocks[parentBlockIndex].elapsedTimeChildren += elapsedTime;
		Profiler::parentBlockIndex = parentBlockIndex;
	}

private:
	const char* blockName;
	u64 beginCpuTime;
	u64 elapsedTimeAtRoot;

	u32 parentBlockIndex;
	static u32 blockIndex;
};

template <const char* N, const unsigned ID>
u32 ProfileBlock<N, ID>::blockIndex = 0;


class Profiler
{
	template <const char* N, const unsigned ID>
	friend class ProfileBlock;

public:
	static void Begin();
	static void End();

	static void PrintBlocks();

private:
	static u32 GetNewIndex();

private:
	static ProfilerBlockInfo blocks[4096];
	static u64 beginCpuTime;
	static u64 endCpuTime;

	static u32 indexCounter;
	static u32 parentBlockIndex;
};
