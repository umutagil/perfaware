#include "repetition_tester.h"

#include "platform_metrics.h"
#include "read_test.h"

#include <stdio.h>
#include <assert.h>

static f64 SecondsFromCPUTime(u64 cpuTime, u64 cpuTimerFreq)
{
	f64 result = 0.0;
	if (cpuTimerFreq) {
		result = (static_cast<f64>(cpuTime) / static_cast<f64>(cpuTimerFreq));
	}

	return result;
}

static f64 ToMegabyte(const u64 bytes)
{
	return static_cast<f64>(bytes) / (1024.0 * 1024.0);
}

static f64 ToGigabyte(const u64 bytes)
{
	return static_cast<f64>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

static std::string GetAllocationDescription(const AllocationType type)
{
	switch (type) {
		case AllocationType::None:
			return "no allocation";
		case AllocationType::VectorResize:
			return "vector resize";
		default:
			return "Unrecognized allocation type";
	}
}

static void PrintRepetitionValue(const char* label, const RepetitionValue& repVal, const u64 cpuFreq)
{
	assert(cpuFreq);
	const f64 durationSec = SecondsFromCPUTime(repVal.elapsedCpuTime, cpuFreq);
	printf("%s: %llu (%fms)", label, repVal.elapsedCpuTime, durationSec * 1000.0f);

	if (repVal.byteCount > 0) {
		const f64 gigabytesPerSec = ToGigabyte(repVal.byteCount) / durationSec;
		printf(" %fgb/s", gigabytesPerSec);
	}

	if (repVal.memPageFaults > 0) {
		printf(" PF: %llu (%0.4fk/fault)", repVal.memPageFaults, static_cast<f64>(repVal.byteCount) / (repVal.memPageFaults * 1024.0));
	}
}

void RepetitionTester::DoTest()
{
	testsStartedAt = ReadCPUTimer();

	TestResult res{};
	while (IsTesting()) {
		res = test.func(test.params);

		if (res.isError) {
			HandleError("Error TODO: reason");
			continue;
		}

		bytesAccumulatedOnThisTest += res.value.byteCount;

		++stats.testCount;
		stats.total += res.value;

		if (stats.max.elapsedCpuTime < res.value.elapsedCpuTime) {
			stats.max = res.value;
		}

		if (stats.min.elapsedCpuTime > res.value.elapsedCpuTime) {
			stats.min = res.value;
			testsStartedAt = ReadCPUTimer();

			PrintRepetitionValue("Min", stats.min, cpuTimerFreq);
			printf("                                          \r");
		}
	}

	// act according to result
	if (mode == TestMode::Completed) {
		PrintResults();
	}
}

bool RepetitionTester::IsTesting()
{
	if (mode == TestMode::Testing) {
		if (stats.testCount && (targetProcessedByteCount != bytesAccumulatedOnThisTest)) {
			HandleError("Processed byte count mismatch!");
		}

		if (mode == TestMode::Testing) {
			bytesAccumulatedOnThisTest = 0;
		}

		const u64 cpuTime = ReadCPUTimer();
		if (cpuTime > testsStartedAt + tryForTime) {
			mode = TestMode::Completed;
		}
	}

	return mode == TestMode::Testing;
}

void RepetitionTester::PrintResults()
{
	PrintRepetitionValue("Min", stats.min, cpuTimerFreq);
	printf("\n");

	PrintRepetitionValue("Max", stats.max, cpuTimerFreq);
	printf("\n");

	if (stats.testCount) {
		PrintRepetitionValue("Avg", stats.total.GetAverage(stats.testCount), cpuTimerFreq);
		printf("\n");
	}
}

void RepetitionTester::HandleError(const char* message)
{
	mode = TestMode::Error;
	fprintf(stderr, "ERROR: %s\n", message);
}

void RepetitionTester::NewTestWave(const u64 byteCount, const AllocationType allocationType, const u32 secondsToTry)
{
	if (mode == TestMode::Uninitialized) {
		mode = TestMode::Testing;
		targetProcessedByteCount = byteCount;
		bytesAccumulatedOnThisTest = 0;
	}
	else if (mode == TestMode::Completed) {
		// TODO(Umut): Repeating tests for variable cpu frequency and byte count.
	}

	tryForTime = secondsToTry * cpuTimerFreq;

	printf("--- %s (%s) ---\n", test.name.c_str(), GetAllocationDescription(allocationType).c_str());
}

RepetitionValue RepetitionValue::GetAverage(const size_t count) const
{
	const f64 divisor = static_cast<f64>(count);
	return RepetitionValue{ static_cast<u64>(elapsedCpuTime / divisor),
							static_cast<u64>(memPageFaults / divisor),
							static_cast<u64>(byteCount / divisor)
						  };
}

RepetitionValue& RepetitionValue::operator+=(const RepetitionValue& rhs)
{
	elapsedCpuTime += rhs.elapsedCpuTime;
	memPageFaults += rhs.memPageFaults;
	byteCount += rhs.byteCount;
	return *this;
}
