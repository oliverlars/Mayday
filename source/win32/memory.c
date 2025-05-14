#include <windows.h>
#include "memory.h"

per_thread_local per_thread_context __ThreadLocalContext;

m_arena *ArenaInitialise(s64 NumberOfBytes)
{
	m_arena Arena = {0};
	Arena.Start = MemoryReserve(NumberOfBytes + GetPageSize());
	m_arena *Result = ArenaPushArray(&Arena, m_arena, 1);
	Arena.Start += GetPageSize();
	Arena.At = 0;
	Arena.Committed = 0;

	Result[0] = Arena;
	return Result;
}

m_scratch_arena ScratchBegin(m_arena **Conflicts, int ConflictCount)
{
	per_thread_context *ThreadContext = GetThreadContext();
	m_scratch_arena Scratch = {0};
	for(int ScratchIndex = 0;
		ScratchIndex < ArrayCount(ThreadContext->ScratchArenas);
		ScratchIndex++)
	{
		b32 IsConflicting = False;
		for(m_arena **Conflict = Conflicts;
			Conflict < Conflicts + ConflictCount;
			Conflict++)
		{
			if(Conflict[0] == ThreadContext->ScratchArenas[ScratchIndex])
			{
				IsConflicting = True;
				break;
			}
		}
		if(!IsConflicting)
		{
			Scratch.Arena = ThreadContext->ScratchArenas[ScratchIndex];
			Scratch.At = Scratch.Arena->At;
			break;
		}
	}
	return Scratch;
}

void ScratchEnd(m_scratch_arena Scratch)
{
	Scratch.Arena->At = Scratch.At;
}

per_thread_context *GetThreadLocalContext()
{
	return &__ThreadLocalContext;
}


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

u8* ArenaPush(m_arena *Arena, s64 Bytes)
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

u8* ArenaPushZero(m_arena *Arena, s64 Bytes)
{
	u8* Memory = ArenaPush(Arena, Bytes);
	ZeroMemory(Memory, Bytes);
	return Memory;
}