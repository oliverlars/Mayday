#pragma once
#include "misc.h"

typedef struct memory_arena
{
	u8 *Start;
	s64 Committed;
	s64 At;
}
m_arena;

typedef struct m_scratch_arena
{
	m_arena *Arena;
	s64 At;
} m_scratch_arena;

typedef struct logger logger;

typedef struct per_thread_context
{
	m_arena *LogArena;
	logger *Logger;

	// NOTE(Oliver): We will rely solely on the assumption that functions
	// will only take a single arena as argument.
	m_arena *ScratchArenas[2];

} per_thread_context;

per_thread_context *GetThreadContext();

m_scratch_arena ScratchBegin(m_arena **Arenas, int ConflictCount);
void ScratchEnd(m_scratch_arena Scratch);

void MemorySet(void *Destination, int Byte, s64 NumberOfBytes);
b32 MemoryEqual(void *A, void *B, s64 NumberOfBytes);
void MemoryCopy(void *Destination, void *Source, s64 NumberOfBytes);
u8* MemoryReserve(s64 NumberOfBytes);
void MemoryCommit(void *Address, s64 NumberOfBytes);
void MemoryZero(void *Destination, s64 NumberOfBytes);

#define ArenaPushType(Arena, Type) (Type*)ArenaPush(Arena, sizeof(Type))
#define ArenaPushTypeZero(Arena, Type) (Type*)ArenaPushZero(Arena, sizeof(Type))

u8* ArenaPush(m_arena *Arena, s64 Bytes);
u8* ArenaPushZero(m_arena *Arena, s64 Bytes);

m_arena *ArenaInitialise(s64 NumberOfBytes);

u64 GetPageSize();

void ArenaReset(m_arena *Arena);