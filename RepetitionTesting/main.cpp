#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <array>

#include "repetition_tester.h"
#include "platform_metrics.h"
#include "read_write_tests.h"


TestInfo readTests[] = {
							TestInfo{ "WriteToAllBytes", WriteToAllBytes, nullptr },
							TestInfo{ "_read", TestReadViaRead, nullptr },
							TestInfo{ "Readfile", TestReadViaReadFile, nullptr },
							TestInfo{ "fread", TestReadViaFRead, nullptr }
						};

TestInfo writeTests[] = {
							TestInfo{ "WriteToAllBytes", WriteToAllBytes, nullptr },
							TestInfo{ "WriteToAllBytesBackward", WriteToAllBytesBackward, nullptr }
						};


void RunTests(const bool infinite)
{
	InitializeOsMetrics();

	ReadTestParameters params{ "data/haversine_data10000000.json" };
	const u64 cpuFreq = GetEstimatedCPUFrequency();
	auto &tests = writeTests;

	const size_t arraySize = ARRAY_SIZE(tests);
	for (size_t testIdx = 0; testIdx < arraySize; ++testIdx) {
		tests[testIdx].params = &params;

		for (size_t allocType = 0; allocType < static_cast<size_t>(AllocationType::Count); ++allocType) {
			params.allocationType = static_cast<AllocationType>(allocType);

			RepetitionTester tester(cpuFreq, tests[testIdx]);
			tester.NewTestWave(params.dest.size(), params.allocationType, 10);
			tester.DoTest();
		}

		if (infinite && (testIdx == arraySize - 1)) {
			testIdx = -1;
		}
	}
}

int main(int ArgCount, char** Args)
{
	RunTests(true);

	return 0;
}









