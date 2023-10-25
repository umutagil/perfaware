#pragma once

#include <source_location>
#include <assert.h>
#include <array>

#include "basedef.h"
#include "platform_metrics.h"


#define GET_LOCATION std::source_location::current()

#define NAME_CONCAT(A, B) A##B
#define CREATE_BLOCK(blockName, varNamePostfix) const auto NAME_CONCAT(block, varNamePostfix) = GetProfileBlock<GET_LOCATION, GET_LOCATION>(blockName);
#define PROFILE_BLOCK(name) CREATE_BLOCK(name, __COUNTER__)
#define PROFILE_BLOCK_FUNCTION PROFILE_BLOCK(__func__)

struct SubStr
{
	constexpr SubStr(const size_t start, const size_t len)
		: startIdx{ start }
		, length{ len }
	{
	}

	size_t startIdx;
	size_t length;
};

constexpr SubStr LastSubstring(const char* str, const char delimeter = '\\')
{
	size_t startIdx = 0;
	size_t len = 0;
	for (size_t i = 0; str[i] != '\0'; ++i, ++len) {
		if (str[i] == delimeter) {
			len = 0;
			startIdx = i;
		}
	}

	assert(len > 0);
	return SubStr{startIdx, len - 1};
}

struct SubStringInfo
{
	constexpr SubStringInfo(std::source_location const& loc)
		: substring{ LastSubstring(loc.file_name()) }
	{
	}

	SubStr substring;
};

template<SubStringInfo S>
struct SourceLocationInfo
{
	constexpr SourceLocationInfo(std::source_location const& loc)
		: fileNameArray{}
		, lineNum{ loc.line() }
	{
		const char* fileName = loc.file_name();
		const char* start = fileName + S.substring.startIdx;
		const char* end = start + S.substring.length;
		std::copy(start, end, fileNameArray.begin());
	}

	std::array<char, S.substring.length> fileNameArray;
	unsigned lineNum;
};

class Profiler;

struct ProfilerBlockInfo
{
	const char* name;
	u64 elapsedTime = 0;
	u64 elapsedTimeChildren = 0;
	u64 elapsedTimeRoot = 0;
	u64 hitCount = 0;
};

template <SourceLocationInfo S>
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

template <SourceLocationInfo S>
u32 ProfileBlock<S>::blockIndex = 0;


class Profiler
{
	template <SourceLocationInfo S>
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


template<SubStringInfo S, SourceLocationInfo<S> A>
constexpr auto GetProfileBlock(const char* blockName)
{
	return ProfileBlock<A>(blockName);
}