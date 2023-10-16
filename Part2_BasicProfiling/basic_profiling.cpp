// Part2_BasicProfiling.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <string>
#include <format>
#include <filesystem>

#include "haversine_formula.h"
#include "parser.h"
#include "platform_metrics.h"

std::random_device randomDevice;
std::mt19937_64 generator(randomDevice());
std::uniform_real_distribution<f64> distGlobalX(-180.0, 180.0);
std::uniform_real_distribution<f64> distGlobalY(-90.0, 90.0);

const char* DATA_FILE_NAME_BASE = "haversine_data";
const char* DATA_FILE_NAME_EXT = ".json";

const char* ANSWERS_FILE_NAME_BASE = "haversine_answers";
const char* ANSWERS_FILE_NAME_EXT = ".f64";

const u64 NUM_PAIRS = 1000000;
const unsigned CLUSTER_COUNT = 32;

double wrapToRange(double value, double min, double max)
{
	const double range = max - min;
	return (value < 0 ? max : min) + fmod(value, range);
}

inline Point GetGlobalRandomPoint()
{
	return { distGlobalX(generator), distGlobalY(generator) };
}

inline f64 GetRandom(const f64 min, const f64 max)
{
	std::uniform_real_distribution<f64> dist(min, max);
	return dist(generator);
}

HaversinePair CreateRandomPair(std::uniform_real_distribution<f64>& distX, std::uniform_real_distribution<f64>& distY)
{
	return {
		wrapToRange(distX(generator), -180.0, 180.0),
		wrapToRange(distY(generator), -90.0, 90.0),
		wrapToRange(distX(generator), -180.0, 180.0),
		wrapToRange(distY(generator), -90.0, 90.0)
	};
}

inline void PrintPair(const HaversinePair& pair)
{
	std::cout << "x0: " << pair.p0.x << ", y0: " << pair.p0.y;
	std::cout << ", x1: " << pair.p1.x << ", y1: " << pair.p1.y;
}

std::vector<HaversinePair> CreateCluster(const Point center, const f64 xRange, const f64 yRange, const unsigned numPairs)
{
	std::uniform_real_distribution<f64> distX(center.x - xRange, center.x + xRange);
	std::uniform_real_distribution<f64> distY(center.y - yRange, center.y + yRange);

	std::vector<HaversinePair> pairs;
	pairs.resize(numPairs);
	std::generate(pairs.begin(), pairs.end(), [&]() -> HaversinePair { return CreateRandomPair(distX, distY); });
	return pairs;
}

std::vector<HaversinePair> CreatePairsCluster()
{
	std::vector<HaversinePair> pairs;
	pairs.reserve(NUM_PAIRS);
	unsigned clusterPopulation = NUM_PAIRS / CLUSTER_COUNT;

	for (size_t i = 0; i < CLUSTER_COUNT; ++i) {
		if (i == CLUSTER_COUNT - 1) {
			clusterPopulation = NUM_PAIRS - pairs.size();
		}

		const Point center = GetGlobalRandomPoint();
		const f64 xRange = GetRandom(0, 180);
		const f64 yRange = GetRandom(0, 90);
		std::vector<HaversinePair> cluster = CreateCluster(center, xRange, yRange, clusterPopulation);
		pairs.insert(pairs.end(), std::make_move_iterator(cluster.begin()), std::make_move_iterator(cluster.end()));
	}

	return pairs;
}

std::vector<HaversinePair> CreatePairsUniform()
{
	std::vector<HaversinePair> pairs;
	pairs.resize(NUM_PAIRS);
	std::generate(pairs.begin(), pairs.end(), [&]() { return CreateRandomPair(distGlobalX, distGlobalY); });
	return pairs;
}

std::vector<HaversinePair> CreatePairs(const bool isCluster)
{
	if (!isCluster) {
		return CreatePairsUniform();
	}

	return CreatePairsCluster();
}

f64 ComputeMeanDistance(const std::vector<HaversinePair>& pairs)
{
	const f64 coef = 1.0 / static_cast<f64>(pairs.size());
	f64 mean = 0;
	for (const HaversinePair& pair : pairs) {
		mean += coef * ReferenceHaversine(pair.p0.x, pair.p0.y, pair.p1.x, pair.p1.y, EARTH_RADIUS);
	}

	return mean;
}

void WritePairs(const std::vector<HaversinePair>& pairs)
{
	const std::string data_file_name = DATA_FILE_NAME_BASE + std::to_string(NUM_PAIRS) + DATA_FILE_NAME_EXT;
	FILE* file;
	errno_t err = fopen_s(&file, data_file_name.c_str(), "wb");
	if (err || (!file)) {
		return;
	}

	const std::string answers_file_name = ANSWERS_FILE_NAME_BASE + std::to_string(NUM_PAIRS) + ANSWERS_FILE_NAME_EXT;
	FILE* fileAnswers;
	errno_t errAnswers = fopen_s(&fileAnswers, answers_file_name.c_str(), "wb");
	if (errAnswers || (!fileAnswers)) {
		return;
	}

	fprintf(file, "{\"pairs\":[");

	const size_t pairCount = pairs.size();
	const f64 coef = 1.0 / static_cast<f64>(pairCount);
	f64 mean = 0;

	for (size_t i = 0; i < pairCount; ++i) {
		const HaversinePair& pair = pairs[i];
		const char* separator = (i < (pairCount - 1)) ? ",\n" : "\n";
		fprintf(file, "{\"x0\":%.16f, \"y0\":%.16f, \"x1\":%.16f, \"y1\":%.16f}%s",
				pair.p0.x, pair.p0.y, pair.p1.x, pair.p1.y, separator);

		const f64 distance = ReferenceHaversine(pair.p0.x, pair.p0.y, pair.p1.x, pair.p1.y, EARTH_RADIUS);
		mean += coef * distance;
		fwrite(&distance, sizeof(f64), 1, fileAnswers);
	}

	fprintf(file, "]}");
	fwrite(&mean, sizeof(f64), 1, fileAnswers);

	fclose(file);
	fclose(fileAnswers);
}

bool ValidateResult(const size_t pairCount, const f64 computedMean, const char* answersFile)
{
	std::ifstream file(answersFile, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	const uintmax_t size = std::filesystem::file_size(answersFile);
	const size_t answerPairCount = (size - sizeof(f64)) / sizeof(f64);
	if (pairCount != answerPairCount) {
		fprintf(stdout, "Pair count does not match!");
		return false;
	}

	char* buffer = new char[size];
	file.read(buffer, size);
	file.close();

	//const f64* answersArray = reinterpret_cast<f64*>(buffer);
	const f64* answersArray = (f64*)(buffer);
	const f64 refMean = answersArray[pairCount];

	fprintf(stdout, "Reference mean: %.16f\n", refMean);
	fprintf(stdout, "Difference: %.16f\n", computedMean - refMean);

	return true;
}

void PrintCpuTime(const char* label, const u64 totalTime, const u64 startTime, const u64 endTime)
{
	const u64 elapsedTime = endTime - startTime;
	const f64 percentage = static_cast<f64>(elapsedTime * 100) / static_cast<f64>(totalTime);
	fprintf(stdout, "%s: %llu (%.2f%%)\n", label,  elapsedTime, percentage);
}

const bool generateData = false;
const bool parseData = true;

int main()
{
	const u64 cpuStart = ReadCPUTimer();
	u64 cpuAfterWrite = cpuStart;

	u64 cpuReadStart = cpuStart;
	u64 cpuReadEnd = cpuStart;

	u64 cpuParseStart = cpuStart;
	u64 cpuParseEnd = cpuStart;

	u64 cpuComputeStart = cpuStart;
	u64 cpuComputeEnd = cpuStart;

	u64 cpuValidateStart = cpuStart;
	u64 cpuValidateEnd = cpuStart;

	if (generateData) {
		const std::vector<HaversinePair> pairs = CreatePairs(true);
		WritePairs(pairs);
		cpuAfterWrite = ReadCPUTimer();
	}

	if (!parseData) {
		return 0;
	}

	cpuReadStart = ReadCPUTimer();
	const std::string data_file_name = DATA_FILE_NAME_BASE + std::string("1000000") + DATA_FILE_NAME_EXT;
	JsonParser parser;
	parser.Read(data_file_name);
	cpuReadEnd = ReadCPUTimer();

	cpuParseStart = cpuReadEnd;
	const std::vector<HaversinePair> parsedPairs = parser.Parse();
	cpuParseEnd = ReadCPUTimer();

	cpuComputeStart = ReadCPUTimer();
	const f64 haversineMean = ComputeMeanDistance(parsedPairs);
	cpuComputeEnd = ReadCPUTimer();

	cpuValidateStart = ReadCPUTimer();
	const bool valid = ValidateResult(parsedPairs.size(), haversineMean, ANSWERS_FILE_NAME_BASE);
	cpuValidateEnd = ReadCPUTimer();

	fprintf(stdout, "Pair count: %llu\n", parsedPairs.size());
	fprintf(stdout, "Haversine mean: %.16f\n", haversineMean);

	const u64 cpuEnd = ReadCPUTimer();
	const u64 cpuFreq = GetEstimatedCPUFrequency();

	const u64 totalCpuElapsed = cpuEnd - cpuStart;
	const f64 totalCpuTime = static_cast<f64>(totalCpuElapsed) * 1000 / static_cast<f64>(cpuFreq);

	fprintf(stdout, "Total time: %.4fms (CPU freq %llu)\n", totalCpuTime, cpuFreq);
	PrintCpuTime("Read", totalCpuElapsed, cpuReadStart, cpuReadEnd);
	PrintCpuTime("Parse", totalCpuElapsed, cpuParseStart, cpuParseEnd);
	PrintCpuTime("Compute", totalCpuElapsed, cpuComputeStart, cpuComputeEnd);
	PrintCpuTime("Validate", totalCpuElapsed, cpuValidateStart, cpuValidateEnd);

}

