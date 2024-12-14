#include "memory.h"
#include "misc.h"

#include <sys/mman.h>
#include <unistd.h>

u8* MemoryReserve(s64 NumberOfBytes)
{
	return mmap(NULL, NumberOfBytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void MemoryCommit(void *Address, s64 NumberOfBytes) 
{
	mprotect(Address, NumberOfBytes, PROT_READ|PROT_WRITE);
}

void MemoryCopy(void *Destination, void *Source, s64 NumberOfBytes)
{
	memcpy(Destination, Source, NumberOfBytes);
}

void MemoryZero(void *Destination, s64 NumberOfBytes)
{
	memset(Destination, 0, NumberOfBytes);
}

u64 GetPageSize()
{
	return getpagesize();
}

u8* ArenaPush(memory_arena *Arena, s64 Bytes)
{
	if(Arena->Start == Null)
	{
		Arena->Start = MemoryReserve(GigaBytes(16));
	}
	if(Arena->At + Bytes >= Arena->Committed)
	{
		s64 BytesNeeded = (Arena->At + Bytes - Arena->Committed);
		u64 BytesToCommit = BytesNeeded + GetPageSize() - (BytesNeeded % GetPageSize());
		MemoryCommit(Arena->Start + Arena->Committed, BytesToCommit);
		Arena->Committed += BytesToCommit;
	}
	u8* Memory = Arena->Start + Arena->At;
	Arena->At += Bytes;
	
	return Memory;
}

u8 *ArenaPushZero(memory_arena *Arena, s64 Bytes)
{
	u8 *Memory = ArenaPush(Arena, Bytes);
	MemoryZero(Memory, Bytes);
	return Memory;
}