#pragma once

#include <stdlib.h>
#include <string.h>
#include "misc.h"

typedef struct memory_arena
{
	u8 *Start;
	s64 Committed;
	s64 At;
}
memory_arena;

void MemorySet(void *Destination, int Byte, s64 NumberOfBytes);
void MemoryCopy(void *Destination, void *Source, s64 NumberOfBytes);
u8* MemoryReserve(s64 NumberOfBytes);
void MemoryCommit(void *Address, s64 NumberOfBytes);
void MemoryZero(void *Destination, s64 NumberOfBytes);

#define ArenaPushType(Arena, Type) (Type*)ArenaPush(Arena, sizeof(Type))
#define ArenaPushTypeZero(Arena, Type) (Type*)ArenaPushZero(Arena, sizeof(Type))

u8* ArenaPush(memory_arena *Arena, s64 Bytes);
u8* ArenaPushZero(memory_arena *Arena, s64 Bytes);


void ArenaReset(memory_arena *Arena);