#pragma once
#include <stdatomic.h>
#include <pthread.h>

#include "memory.h"
#include "misc.h"

#define DispatchFunction(Name, Data) void *DispatchFunction(void *Data)

typedef struct barrier
{
	atomic_ullong Generation;
	atomic_ullong LanesWaiting;
	s64 LaneCount;
}
barrier;

typedef struct wave_info wave_info;

typedef struct shared_wave_info
{
	s32 ArgCount;
	char **Args;
	barrier *Barrier;
	barrier *TaskBarrier;

	s64 LaneCount;

	m_arena *Arena;

	atomic_llong Data[4096];

	atomic_int WorkAvailable;
	void (*Entry)(wave_info*, void*);
	void *EntryData;

}
shared_wave_info;

typedef struct wave_info
{
	shared_wave_info *Shared;
	s32 Lane;
}
wave_info;

void DispatchInitialise(shared_wave_info *Shared, s64 LaneCount);
void Dispatch(shared_wave_info *Shared, void (*Entry)(wave_info* WaveInfo, void*), void* EntryData);

void GetLaneCount();

void WaveBarrier(barrier *Barrier);

b32 WaveIsFirstLane(wave_info *WaveInfo);

void BroadcastBytes(wave_info *WaveInfo, void *Bytes, s64 ByteCount);

#define BroadcastToAllLanes(WaveInfo, Variable) BroadcastBytes(WaveInfo, &Variable, sizeof(Variable))

s64 WaveGetLaneIndex(wave_info *WaveInfo);

u64 NearestPowerOfTwo(u64 Value);

u64 InclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value);
u64 ExclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value);

u64 WaveSum(wave_info *WaveInfo, u64 Value);

b32 WaveAnyTrue(wave_info *WaveInfo, b32 Expression);

b32 WaveAllTrue(wave_info *WaveInfo, b32 Expression);

s64 LaneValueFromTotal(wave_info *WaveInfo, s64 Count);

s64 AtomicAdd(s64 *Value, s64 Addend);
