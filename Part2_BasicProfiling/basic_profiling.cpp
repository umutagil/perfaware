// Part2_BasicProfiling.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <string>
#include <format>
#include <chrono>
#include <filesystem>

#include "haversine_formula.h"
#include "parser.h"

std::random_device randomDevice;
std::mt19937_64 generator(randomDevice());
std::uniform_real_distribution<f64> distGlobalX(-180.0, 180.0);
std::uniform_real_distribution<f64> distGlobalY(-90.0, 90.0);

const char* DATA_FILE_NAME = "haversine_data.json";
const char* ANSWERS_FILE_NAME = "haversine_answers.f64";

const u64 NUM_PAIRS = 100000;
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
	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

	FILE* file;
	errno_t err = fopen_s(&file, DATA_FILE_NAME, "wb");
	if (err || (!file)) {
		return;
	}

	FILE* fileAnswers;
	errno_t errAnswers = fopen_s(&fileAnswers, ANSWERS_FILE_NAME, "wb");
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

	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	std::cout << "elapsed time data file generation: = " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "[ms]" << std::endl;
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

const bool generateData = true;
const bool parseData = true;
const bool computeMeanHaversine = true;

int main()
{
	if (generateData) {
		const std::vector<HaversinePair> pairs = CreatePairs(true);
		WritePairs(pairs);
	}

	if (parseData) {
		std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

		JsonParser parser;
		const std::vector<HaversinePair> parsedPairs = parser.Parse(DATA_FILE_NAME);

		std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		std::cout << "elapsed time json parsing: = " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "[ms]" << std::endl;

		if (computeMeanHaversine) {
			std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

			const f64 haversineMean = ComputeMeanDistance(parsedPairs);
			const bool valid = ValidateResult(parsedPairs.size(), haversineMean, ANSWERS_FILE_NAME);

			std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();
			std::cout << "Elapsed time computing haversine: = " << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << "[ms]" << std::endl;
		}
	}
}

