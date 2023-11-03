#pragma once

#include <string>
#include <limits.h>

#include "../Part2_BasicProfiling/basedef.h"

enum class AllocationType : u8;

struct ITestParameters
{
	//virtual ~ITestParameters() {}
};

struct TestResult;
using TestFunction = TestResult (*)(ITestParameters* params);

struct TestInfo
{
	TestInfo()
		: name("empty")
		, func(nullptr)
		, params(nullptr)

	{
	}

	TestInfo(std::string&& testName, TestFunction testFunc, ITestParameters* testParams)
		: name(testName)
		, func(testFunc)
		, params(testParams)
	{
	}

	std::string name;
	TestFunction func;
	ITestParameters* params;
};

enum class TestMode : u8
{
	Uninitialized,
	Testing,
	Completed,
	Error,
};

struct RepetitionValue
{
	u64 elapsedCpuTime = 0;
	u64 memPageFaults = 0;
	u64 byteCount = 0;

	RepetitionValue GetAverage(const size_t count) const;
	RepetitionValue& operator+= (const RepetitionValue& rhs);
};

struct TestResult
{
	bool isError = false;
	RepetitionValue value;
};

struct RepetitionStats
{
	u64 testCount = 0;
	RepetitionValue total;
	RepetitionValue max;
	RepetitionValue min = RepetitionValue{ ULLONG_MAX, 0, 0 };
};

struct RepetitionTester
{
	RepetitionTester(const u64 cpuFreq, const TestInfo& testInfo)
		: targetProcessedByteCount(0)
		, cpuTimerFreq(cpuFreq)
		, tryForTime(0)
		, testsStartedAt(0)
		, bytesAccumulatedOnThisTest(0)
		, mode (TestMode::Uninitialized)
		, test(testInfo)
		, stats()
	{
	}

	void DoTest();

	bool IsTesting();
	void PrintResults();
	void HandleError(const char* message);
	void NewTestWave(const u64 byteCount, const AllocationType allocationType, const u32 secondsToTry = 10);

	u64 targetProcessedByteCount;
	u64 cpuTimerFreq;
	u64 tryForTime;
	u64 testsStartedAt;

	u64 bytesAccumulatedOnThisTest;

	TestMode mode;
	TestInfo test;
	RepetitionStats stats;
};


