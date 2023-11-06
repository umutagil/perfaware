#pragma once

#include "../Part2_BasicProfiling/basedef.h"

struct DecomposedVirtualAddress
{
	u16 pml4Index;
	u16 directoryPtrIndex;
	u16 directoryIndex;
	u16 tableIndex;
	u32 offset;
};


DecomposedVirtualAddress DecomposePointer4K(void* ptr);
DecomposedVirtualAddress DecomposePointer2MB(void* ptr);
DecomposedVirtualAddress DecomposePointer1GB(void* ptr);

void PrintVirtualAddress(const char* label, const DecomposedVirtualAddress address);
void PrintAddressIndicesLowerHalf(const DecomposedVirtualAddress address);

void DoVirtualAddressAnalysis();


