#pragma once

#include <source_location>
#include <assert.h>
#include <array>
#include <functional>

#include "basedef.h"
#include "platform_metrics.h"

using TimerFunction = std::function<u64()>;
using CpuFrequencyFunction = std::function<u64()>;

#ifndef ALTERNATE_TIMER
#define ALTERNATE_TIMER 0
#endif

#if ALTERNATE_TIMER
	inline TimerFunction TimerFunc = ReadOSTimer;
	inline CpuFrequencyFunction CpuFrequencyFunc = GetOSTimerFreq;
#else
	inline TimerFunction TimerFunc = ReadCPUTimer;
	inline CpuFrequencyFunction CpuFrequencyFunc = GetEstimatedCPUFrequency;
#endif

#ifndef PROFILER
#define PROFILER 0
#endif // !PROFILER

#if PROFILER

#define GET_LOCATION std::source_location::current()

#define CONCAT2(A, B) A##B
#define	NAME_CONCAT(A, B) CONCAT2(A, B)

#define PROFILE_BLOCK(blockName, ...) const auto NAME_CONCAT(block, __COUNTER__) = GetProfileBlock<GET_LOCATION, GET_LOCATION>(blockName, __VA_ARGS__);
#define PROFILE_BLOCK_FUNCTION(...) PROFILE_BLOCK(__func__, __VA_ARGS__)

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

struct Runner
{
	Runner(auto&& callable)
	{
		callable();
	}
};

class Profiler;

struct ProfilerBlockInfo
{
	const char* name;
	u64 elapsedTimeExclusive;
	u64 elapsedTimeInclusive;
	u64 hitCount;
	u64 processedByteCount;
};

// NOTE(Umut): Block name can be even retrieved as template parameter.
template <SourceLocationInfo S>
class ProfileBlock
{
public:

	ProfileBlock(const char* blockName, const u64 byteCount = 0)
		: beginCpuTime(TimerFunc())
		, parentBlockIndex(Profiler::parentBlockIndex)
		, elapsedTimeInclusive(Profiler::blocks[blockIndex].elapsedTimeInclusive)
	{
		static const Runner initialTask{ [&blockName]() {
			blockIndex = Profiler::GetNewIndex();
			Profiler::blocks[blockIndex].name = blockName;
		}};

		Profiler::blocks[blockIndex].processedByteCount += byteCount;
		Profiler::parentBlockIndex = blockIndex;
	}

	~ProfileBlock()
	{
		const u64 elapsedTime = TimerFunc() - beginCpuTime;
		ProfilerBlockInfo& block(Profiler::blocks[blockIndex]);
		++block.hitCount;
		block.elapsedTimeExclusive += elapsedTime;
		block.elapsedTimeInclusive = elapsedTimeInclusive + elapsedTime;

		Profiler::blocks[parentBlockIndex].elapsedTimeExclusive -= elapsedTime;
		Profiler::parentBlockIndex = parentBlockIndex;
	}

private:
	u64 beginCpuTime;
	u64 elapsedTimeInclusive;

	u32 parentBlockIndex;
	static u32 blockIndex;
};

template <SourceLocationInfo S>
u32 ProfileBlock<S>::blockIndex = 0;

/**
 * @brief Generate a profile block that profiles CPU and bandwith during its scope.
 *
 * @blockName Name of the block.
 * @byteCount Number of bytes used (to measure bandwith).
 */
template<SubStringInfo S, SourceLocationInfo<S> A>
constexpr auto GetProfileBlock(const char* blockName, const u64 byteCount = 0)
{
	return ProfileBlock<A>(blockName, byteCount);
}

#else
#define PROFILE_BLOCK(...)
#define PROFILE_BLOCK_FUNCTION PROFILE_BLOCK(...)

struct SourceLocationInfo{};
struct ProfilerBlockInfo{};

#endif


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