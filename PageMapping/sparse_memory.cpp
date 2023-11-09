#include "sparse_memory.h"

#include "basedef.h"

struct SparseBuffer
{
	u8* data = nullptr;
	u64 size = 0;
};

static b32 IsValid(const SparseBuffer& buffer)
{
	return !!buffer.data;
}

static SparseBuffer AllocateSparseBuffer(u64 size)
{
	SparseBuffer result = {};

	result.data = (u8*)VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
	if (result.data) {
		result.size = size;
	}

	return result;
}

static void DeallocateSparseBuffer(SparseBuffer* buffer)
{
	if (buffer) {
		VirtualFree(buffer->data, 0, MEM_RELEASE);
		*buffer = {};
	}
}

static void EnsureMemoryIsMapped(SparseBuffer*, void* pointer, u32 size)
{
	VirtualAlloc(pointer, size, MEM_COMMIT, PAGE_READWRITE);
}


void RunSparseMemoryTest()
{
	printf("\nSparse memory test:\n");

	const u64 GIGABYTE = 1024*1024*1024;
	SparseBuffer sparse = AllocateSparseBuffer(256 * GIGABYTE);
	if(IsValid(sparse))
	{
		u8* write = sparse.data;
		const u64 offsets[] = {16 * GIGABYTE, 100 * GIGABYTE, 200 * GIGABYTE, 255 * GIGABYTE};

		for(u32 OffsetIndex = 0; OffsetIndex < ARRAY_SIZE(offsets); ++OffsetIndex) {
			const u64 Offset = offsets[OffsetIndex];
			EnsureMemoryIsMapped(&sparse, write + Offset, sizeof(*write));
			write[Offset] = (u8)(100 + OffsetIndex);
		}

		for(u32 OffsetIndex = 0; OffsetIndex < ARRAY_SIZE(offsets); ++OffsetIndex) {
			const u64 Offset = offsets[OffsetIndex];
			printf("  %u: %u\n", OffsetIndex, write[Offset]);
		}
	}

	DeallocateSparseBuffer(&sparse);
}
