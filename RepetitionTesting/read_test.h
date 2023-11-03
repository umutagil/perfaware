#pragma once

#include "repetition_tester.h"

#include <vector>

enum class AllocationType : u8
{
	None,
	VectorResize,

	Count
};

struct ReadTestParameters : ITestParameters
{
	ReadTestParameters(const char* fileName);

	const char* fileName;
	AllocationType allocationType;
	std::vector<char> dest;
};

TestResult TestReadViaIfstream(ITestParameters* params);
TestResult TestReadViaFRead(ITestParameters* params);
TestResult TestReadViaRead(ITestParameters* params);
TestResult TestReadViaReadFile(ITestParameters* params);
TestResult WriteToAllBytes(ITestParameters* params);


