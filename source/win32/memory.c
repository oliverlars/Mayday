#include "memory.h"
#include <windows.h>

u8 *MemoryReserve(s64 NumberOfBytes)
{
    return (u8*)VirtualAlloc((void*)0, NumberOfBytes, MEM_RESERVE, PAGE_READWRITE);
}

void MemoryCommit(void *Address, s64 NumberOfBytes)
{
    VirtualAlloc(Address, NumberOfBytes, MEM_COMMIT, PAGE_READWRITE);
}

void MemoryCopy(void *Destination, void *Source, s64 NumberOfBytes)
{
    RtlCopyMemory(Destination, Source, NumberOfBytes);
}

u64 GetPageSize()
{
    SYSTEM_INFO SystemInfo = {0};
    GetSystemInfo(&SystemInfo);

    return SystemInfo.dwPageSize;
}

u8* ArenaPush(memory_arena *Arena, s64 Bytes)
{
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

u8* ArenaPushZero(memory_arena *Arena, s64 Bytes)
{
    u8* Memory = ArenaPush(Arena, Bytes);
    ZeroMemory(Memory, Bytes);
    return Memory;
}