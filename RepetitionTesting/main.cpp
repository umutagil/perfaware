#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <array>

#include "repetition_tester.h"
#include "platform_metrics.h"
#include "read_test.h"

void RunTests(const bool infinite)
{
	InitializeOsMetrics();

	ReadTestParameters params{ "data/haversine_data10000000.json" };
	const u64 cpuFreq = GetEstimatedCPUFrequency();
	const std::array<TestInfo, 4>tests = {
										   TestInfo{ "WriteToAllBytes", WriteToAllBytes, &params },
										   TestInfo{ "_read", TestReadViaRead, &params },
										   TestInfo{ "Readfile", TestReadViaReadFile, &params },
										   TestInfo{ "fread", TestReadViaFRead, &params }
										 };

	for (size_t testIdx = 0; testIdx < tests.size(); ++testIdx) {
		for (size_t allocType = 0; allocType < static_cast<size_t>(AllocationType::Count); ++allocType) {
			params.allocationType = static_cast<AllocationType>(allocType);

			RepetitionTester tester(cpuFreq, tests[testIdx]);
			tester.NewTestWave(params.dest.size(), params.allocationType, 10);
			tester.DoTest();
		}

		if (infinite && (testIdx == tests.size() - 1)) {
			testIdx = 0;
		}
	}
}

int main(int ArgCount, char** Args)
{
	RunTests(true);

	return 0;
}









