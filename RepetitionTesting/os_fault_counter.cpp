#include "os_fault_counter.h"
#include "../Part2_BasicProfiling/basedef.h"
#include "platform_metrics.h"
#include "virtual_address_analysis.h"

#include <stdio.h>

void WriteToBuffer(u8* buffer, const u64 size)
{
	for (u64 index = 0; index < size; ++index) {
		buffer[index] = static_cast<u8>(index);
	}
}

void WriteToBufferBackward(u8* buffer, const u64 size)
{
	for (u64 index = 0; index < size; ++index) {
		buffer[size - index - 1] = static_cast<u8>(index);
	}
}

void TestPageFaultCounter(const bool forward)
{
	InitializeOsMetrics();

	const u64 pageSize = 4096;
	const u64 pageCount = 4096;
	const u64 totalSize = pageCount * pageSize;

	printf("Page Count, Touch Count, Fault Count, Extra Faults, Dir Index, Table Index\n");

	for (u64 touchCount = 0; touchCount <= pageCount; ++touchCount) {

		const u64 touchSize = touchCount * pageSize;
		u8* data = static_cast<u8*>(VirtualAlloc(0, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

		if (touchCount == 0) {
			const DecomposedVirtualAddress address = DecomposePointer4K(data);
			PrintVirtualAddress("4K: ", address);
		}

		if (!data) {
			fprintf(stderr, "ERROR: Unable to allocate memory\n");
			continue;
		}

		const u64 startFaultCount = ReadOsPageFaultCount();
		if (forward) {
			WriteToBuffer(data, touchSize);
		}
		else {
			WriteToBufferBackward(data, touchSize);
		}
		const u64 endFaultCount = ReadOsPageFaultCount();
		const u64 faultCount = endFaultCount - startFaultCount;

		printf("%llu, %llu, %llu, %lld\n", pageCount, touchCount, faultCount, (faultCount - touchCount));

		VirtualFree(data, 0, MEM_RELEASE);
	}


}
