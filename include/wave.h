#pragma once
#include <stdatomic.h>

#include "memory.h"
#include "misc.h"

#define Entry(Data) void *Entry(void *Data)

Entry(Data);

typedef struct barrier
{
	atomic_ullong Generation;
	atomic_ullong LanesWaiting;
	s64 LaneCount;
}
barrier;

typedef struct shared_wave_info
{
	s32 ArgCount;
	char **Args;
	barrier *Barrier;

	s64 LaneCount;

	m_arena *Arena;

	s64 Data[4096];
}
shared_wave_info;

typedef struct wave_info
{
	shared_wave_info *Shared;
	s32 Lane;
}
wave_info;

void Dispatch(shared_wave_info *Shared, s64 LaneCount);

b32 WaveIsFirstLane(wave_info *WaveInfo);

void BroadcastBytes(wave_info *WaveInfo, void *Bytes, s64 ByteCount);

#define BroadcastToAllLanes(WaveInfo, Variable) BroadcastBytes(WaveInfo, &Variable, sizeof(Variable))

s64 WaveGetLaneIndex(wave_info *WaveInfo);

u64 NearestPowerOfTwo(u64 Value);

u64 WavePrefixSum(wave_info *WaveInfo, u64 Value);

u64 WaveSum(wave_info *WaveInfo, u64 Value);

b32 WaveAnyTrue(wave_info *WaveInfo, b32 Expression);

b32 WaveAllTrue(wave_info *WaveInfo, b32 Expression);

s64 AtomicAdd(s64 *Value, s64 Addend);
