#include "virtual_address_analysis.h"

#include <stdio.h>
#include <windows.h>

static void PrintAddress(const DecomposedVirtualAddress address)
{
	printf("|%3u|%3u|%3u|%3u|%10u|",
		address.pml4Index, address.directoryPtrIndex,
		address.directoryIndex, address.tableIndex,
		address.offset);
}

void PrintVirtualAddress(const char* label, const DecomposedVirtualAddress address)
{
	printf("%s", label);
	PrintAddress(address);
	printf("\n");
}

DecomposedVirtualAddress DecomposePointer4K(void* ptr)
{
	DecomposedVirtualAddress result = {};

	const u64 address = reinterpret_cast<u64>(ptr);
	result.pml4Index = ((address >> 39) & 0x1ff);
	result.directoryPtrIndex = ((address >> 30) & 0x1ff);
	result.directoryIndex = ((address >> 21) & 0x1ff);
	result.tableIndex = ((address >> 12) & 0x1ff);
	result.offset = ((address >> 0) & 0xfff);

	return result;
}

DecomposedVirtualAddress DecomposePointer2MB(void* ptr)
{
	DecomposedVirtualAddress result = {};

	const u64 address = reinterpret_cast<u64>(ptr);
	result.pml4Index = ((address >> 39) & 0x1ff);
	result.directoryPtrIndex = ((address >> 30) & 0x1ff);
	result.directoryIndex = ((address >> 21) & 0x1ff);
	result.offset = ((address >> 0) & 0x1fffff);

	return result;
}

DecomposedVirtualAddress DecomposePointer1GB(void* ptr)
{
	DecomposedVirtualAddress result = {};

	const u64 address = reinterpret_cast<u64>(ptr);
	result.pml4Index = ((address >> 39) & 0x1ff);
	result.directoryPtrIndex = ((address >> 30) & 0x1ff);
	result.offset = ((address >> 0) & 0x3fffffff);

	return result;
}

void PrintAddressIndicesLowerHalf(const DecomposedVirtualAddress address)
{
	printf("%3u, %3u, %10u",
		address.directoryIndex, address.tableIndex,
		address.offset);
}

static void PrintBinaryBits(u64 value, u32 firstBit, u32 bitCount)
{
	for (u32 bitIndex = 0; bitIndex < bitCount; ++bitIndex) {
		const u64 bit = (value >> ((bitCount - 1 - bitIndex) + firstBit)) & 1;
		printf("%c", bit ? '1' : '0');
	}
}

void DoVirtualAddressAnalysis()
{
	for (int pointerIndex = 0; pointerIndex < 16; ++pointerIndex) {
		void* ptr = static_cast<u8*>(VirtualAlloc(0, 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

		const u64 address = reinterpret_cast<u64>(ptr);
		PrintBinaryBits(address, 48, 16);
		printf("|");
		PrintBinaryBits(address, 39, 9);
		printf("|");
		PrintBinaryBits(address, 30, 9);
		printf("|");
		PrintBinaryBits(address, 21, 9);
		printf("|");
		PrintBinaryBits(address, 12, 9);
		printf("|");
		PrintBinaryBits(address, 0, 12);
		printf("\n");

		PrintVirtualAddress(" 4k paging: ", DecomposePointer4K(ptr));
		PrintVirtualAddress("2mb paging: ", DecomposePointer2MB(ptr));
		PrintVirtualAddress("1gb paging: ", DecomposePointer1GB(ptr));

		printf("\n");
	}
}
