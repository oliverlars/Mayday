#include "memory.h"
#include "misc.h"

#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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

per_thread_context *GetThreadContext()
{
	return &__ThreadLocalContext;
}

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

void MemorySet(void *Destination, int Byte, s64 NumberOfBytes)
{
	memset(Destination, Byte, NumberOfBytes);
}

b32 MemoryEqual(void *A, void *B, s64 NumberOfBytes)
{
	return memcmp(A, B, NumberOfBytes) == 0;
}



u64 GetPageSize()
{
	return getpagesize();
}

b32 IsPowerOfTwo(u64 Value)
{
	b32 Result = False;
	if(Value > 0)
	{
		Result = (Value & (Value-1)) == 0;
	}
	return Result;
}

u64 AlignUp(u64 Pointer, u64 Alignment)
{
	assert(IsPowerOfTwo(Alignment));
	
	u64 Result = Pointer;

	u64 Modulo = Pointer & (Alignment - 1);
	if(Modulo != 0)
	{
		Result += Alignment - Modulo; 
	}
	return Result;
}

u8* ArenaPushAlign(m_arena *Arena, s64 Bytes, u64 Alignment)
{
	u64 AlignedPointer = AlignUp((u64)Arena->At, Alignment);
	s64 Offset = (s64)AlignedPointer - (s64)Arena->At;
	if(Arena->At + Offset + Bytes >= Arena->Committed)
	{
		s64 BytesNeeded = (Arena->At + Offset + Bytes - Arena->Committed);
		u64 BytesToCommit = BytesNeeded + GetPageSize() - (BytesNeeded % GetPageSize());
		MemoryCommit(Arena->Start + Arena->Committed, BytesToCommit);
		Arena->Committed += BytesToCommit;
	}
	u8* Memory = Arena->Start + Arena->At + Offset;
	Arena->At += Offset + Bytes;
	
	return Memory;
}

u8 *ArenaPushAlignZero(m_arena *Arena, s64 Bytes, u64 Alignment)
{
	u8 *Memory = ArenaPushAlign(Arena, Bytes, Alignment);
	MemoryZero(Memory, Bytes);
	return Memory;
}

u8 *ArenaPushZero(m_arena *Arena, s64 Bytes)
{
	return ArenaPushAlignZero(Arena, Bytes, 16);
}

u8 *ArenaPush(m_arena *Arena, s64 Bytes)
{
	return ArenaPushAlign(Arena, Bytes, 16);
}

void ArenaReset(m_arena *Arena)
{
	Arena->At = 0;
}