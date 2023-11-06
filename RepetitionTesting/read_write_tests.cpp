#include "read_write_tests.h"

#include "platform_metrics.h"

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <fstream>
#include <assert.h>
#include <filesystem>
#include <vector>
#include <numeric>

std::vector<char>& HandleAllocation(ReadTestParameters& params, std::vector<char>& localVec)
{
	switch (params.allocationType) {
		case AllocationType::None:
			return params.dest;
		case AllocationType::VectorResize:
			localVec.reserve(params.dest.size());
			return localVec;
		default:
			fprintf(stderr, "ERROR: Unrecognized allocation type.");
			break;
	}

	localVec = std::vector<char>();
	return localVec;
}

static void BeginTime(RepetitionValue& result)
{
	result.elapsedCpuTime -= ReadCPUTimer();
	result.memPageFaults -= ReadOsPageFaultCount();
}

static void EndTime(RepetitionValue& result)
{
	result.elapsedCpuTime += ReadCPUTimer();
	result.memPageFaults += ReadOsPageFaultCount();
}

TestResult TestReadViaIfstream(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	std::ifstream file(readParams->fileName);
	if (!file.is_open()) {
		res.isError = true;
		file.close();
		return res;
	}

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	BeginTime(value);
	file.read(buffer.data(), buffer.capacity());
	EndTime(value);

	file.close();
    return res;
}

TestResult TestReadViaFRead(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	FILE* file;
	errno_t err = fopen_s(&file, readParams->fileName, "rb");
	if (err) {
		res.isError = true;
		return res;
	}

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	BeginTime(value);
	fread(buffer.data(), buffer.capacity(), 1, file);
	EndTime(value);

	fclose(file);
	return res;
}

TestResult TestReadViaRead(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	int fileHandler;
	errno_t err = _sopen_s(&fileHandler, readParams->fileName, _O_BINARY | _O_RDONLY, _SH_DENYNO, _S_IREAD);
	if (err != 0) {
		res.isError = true;
		return res;
	}

	if (readParams->dest.size() > INT_MAX) {
		res.isError = true;
		_close(fileHandler);
		return res;
	}

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	BeginTime(value);
	const int readResult = _read(fileHandler, buffer.data(), buffer.capacity());
	EndTime(value);

	res.isError = !readResult;

	_close(fileHandler);
	return res;
}

TestResult TestReadViaReadFile(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	HANDLE file = CreateFileA(readParams->fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE) {
		res.isError = true;
		return res;
	}

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	DWORD bytesRead = 0;
	BeginTime(value);
	BOOL readResult = ReadFile(file, buffer.data(), buffer.capacity(), &bytesRead, 0);
	EndTime(value);

	res.isError = !readResult;

	CloseHandle(file);
	return res;
}

TestResult WriteToAllBytes(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	BeginTime(value);
	buffer.resize(buffer.capacity());
	const size_t size = buffer.size();
	for (size_t i = 0; i < size; ++i) {
		buffer[i] = static_cast<u8>(i);
	}
	EndTime(value);

	return res;
}

TestResult WriteToAllBytesBackward(ITestParameters* params)
{
	ReadTestParameters* readParams = static_cast<ReadTestParameters*>(params);
	TestResult res{};
	RepetitionValue& value = res.value;

	std::vector<char> tempBuffer;
	std::vector<char>& buffer = HandleAllocation(*readParams, tempBuffer);
	value.byteCount = buffer.capacity();

	BeginTime(value);
	const size_t size = buffer.capacity();
	for (size_t i = 0; i < size; ++i) {
		buffer[size - i - 1] = static_cast<u8>(i);
		printf("%u ", buffer[size - i - 1]);
	}
	EndTime(value);

	printf("\n");

	return res;
}

ReadTestParameters::ReadTestParameters(const char* fileName)
	: fileName(fileName)
	, allocationType(AllocationType::None)
	, dest(std::filesystem::file_size(fileName))
{
}
