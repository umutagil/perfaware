#include "circular_buffer.h"

#include "basedef.h"

#define KERNEL_MEMORY_DLL_PATH L"kernelbase.dll"

struct CircularBuffer
{
	u8* data = nullptr;
	u64 size = 0;
	HANDLE fileMapping = INVALID_HANDLE_VALUE;
	u32 repCount = 0;
};

static b32 IsValid(const CircularBuffer& buffer)
{
	return !!buffer.data;
}

static u64 RoundToPow2Size(const u64 value, const u64 pow2Size)
{
	return (value + (pow2Size - 1)) & ~(pow2Size - 1);
}

static void UnmapCircularBuffer(u8* data, u64 size, u32 repCount)
{
	for (u32 repIdx = 0; repIdx < repCount; ++repIdx) {
		UnmapViewOfFile(data + repIdx * size);
	}
}

static void DeallocateCircularBuffer(CircularBuffer& buffer)
{
	if (buffer.fileMapping != INVALID_HANDLE_VALUE) {
		UnmapCircularBuffer(buffer.data, buffer.size, buffer.repCount);
		CloseHandle(buffer.fileMapping);
	}

	buffer = CircularBuffer{};
}

static CircularBuffer AllocateCircularBuffer(u64 minimumSize, u32 repCount)
{
	CircularBuffer buffer;
	buffer.repCount = repCount;

	SYSTEM_INFO info;
	GetSystemInfo(&info);
	const u64 dataSize = RoundToPow2Size(minimumSize, info.dwAllocationGranularity);
	const u64 totalRepeatedSize = dataSize * repCount;

	buffer.fileMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE,
											(DWORD)(dataSize >> 32), (DWORD)(dataSize & 0xffffffff), 0);

	if (!buffer.fileMapping) {
		return buffer;
	}

	u8* basePtr = (u8*)VirtualAlloc2(0, 0, totalRepeatedSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, 0, 0);
	if (!basePtr) {
		CloseHandle(buffer.fileMapping);
		return buffer;
	}

	bool mapped = true;
	for (u32 repIdx = 0; repIdx < repCount; ++repIdx) {
		VirtualFree(basePtr + (repIdx * dataSize), dataSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);

		mapped &= !!MapViewOfFile3(buffer.fileMapping, 0, basePtr + repIdx * dataSize,
									0, dataSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
	}

	if (mapped) {
		buffer.data = basePtr;
		buffer.size = dataSize;
	}
	else {
		DeallocateCircularBuffer(buffer);
	}

	return buffer;
}

struct BufferHiddenCircular
{
	BufferHiddenCircular(const u64 size)
		: buffer(AllocateCircularBuffer(size, 3))
	{
	}

	~BufferHiddenCircular()
	{
		DeallocateCircularBuffer(buffer);
	}

	auto& operator[](const size_t idx)
	{
		return buffer.data[idx];
	}

	auto GetSize() const
	{
		return buffer.size;
	}

	CircularBuffer buffer;
};

void RunCircularBufferTest()
{
	printf("Circular buffer test:\n");

	BufferHiddenCircular buffer(64 * 4096);
	if (!IsValid(buffer.buffer)) {
		printf("Failed!:\n");
		return;
	}

	const s32 size = static_cast<s32>(buffer.GetSize());
	u8* data = &buffer[0] + size;
	data[0] = 123;

	printf("   [-%8d]: %u\n", -size, data[-size]);
	printf("   [-%8d]: %u\n", 0, data[0]);
	printf("   [-%8d]: %u\n", size, data[size]);
}
