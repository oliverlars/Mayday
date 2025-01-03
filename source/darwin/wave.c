#include "wave.h"
#include "memory.h"

#include <sys/sysctl.h>
#include <os/os_sync_wait_on_address.h>
#include <os/lock.h>
#include <pthread.h>
#include <stdatomic.h>

s64 AtomicAdd(s64 *Value, s64 Addend)
{
	return __sync_fetch_and_add(Value, Addend);
}

/*
Based on Raymond Chen's article on synchronization barriers
https://devblogs.microsoft.com/oldnewthing/20160824-00/?p=94155
*/
void WaveBarrier(barrier *Barrier)
{
	s64 Generation = atomic_load_explicit(&Barrier->Generation, memory_order_acquire);
	s64 LanesWaiting = atomic_fetch_add_explicit(&Barrier->LanesWaiting, 1, memory_order_acquire);
	if (LanesWaiting < Barrier->LaneCount - 1)
	{
		while(Barrier->Generation == Generation)
		{
			os_sync_wait_on_address(&Barrier->Generation, Generation, sizeof(s64), OS_SYNC_WAIT_ON_ADDRESS_SHARED);   
		}
	}
	else if(LanesWaiting + 1 == Barrier->LaneCount)
	{
		Barrier->LanesWaiting = 0;
		atomic_fetch_add_explicit(&Barrier->Generation, 1, memory_order_release);
		os_sync_wake_by_address_all(&Barrier->Generation, sizeof(s64), OS_SYNC_WAKE_BY_ADDRESS_SHARED);
	}
	atomic_thread_fence(memory_order_release);
}

s64 LaneValueFromTotal(wave_info *WaveInfo, s64 Count)
{
	s64 Base = Count / WaveInfo->Shared->LaneCount;
	s64 Remainder = Count % WaveInfo->Shared->LaneCount;
	s64 Result = Count / WaveInfo->Shared->LaneCount + (WaveInfo->Lane < Remainder ? 1 : 0);

	if (Count < WaveInfo->Shared->LaneCount)
	{
		Result = (s64)(WaveInfo->Lane < Count);
	}

	return Result;
}

void Dispatch(shared_wave_info *Shared, s64 LaneCount, void *(*Entry)(void*))
{
	wave_info *Entries = (wave_info*)ArenaPushZero(Shared->Arena, LaneCount*sizeof(wave_info));
	pthread_t *Waves = (pthread_t*)ArenaPushZero(Shared->Arena, LaneCount*sizeof(pthread_t));
	
	barrier Barrier = {0};

	Shared->LaneCount = LaneCount;
	Barrier.LaneCount = LaneCount;
	for (s64 Lane = 0; Lane < LaneCount; Lane++)
	{
		Entries[Lane].Shared = Shared;
		Entries[Lane].Lane = Lane;
		Entries[Lane].Shared->Barrier = &Barrier;
		pthread_create(Waves + Lane, Null, Entry, Entries + Lane);
	}

	for (s64 Lane = 0; Lane < LaneCount; Lane++)
	{
		pthread_join(Waves[Lane], Null);
	} 
}

b32 WaveIsFirstLane(wave_info *WaveInfo)
{
	return WaveInfo->Lane == 0;
}

void BroadcastBytes(wave_info *WaveInfo, void *Bytes, s64 ByteCount)
{
	u8 *Result = Null;
	if(WaveIsFirstLane(WaveInfo))
	{
		m_scratch_arena Scratch = ScratchBegin(0, 0);
		Result = ArenaPushZero(Scratch.Arena, ByteCount);
		MemoryCopy(Result, Bytes, ByteCount);
		ScratchEnd(Scratch);
	}
	WaveBarrier(WaveInfo->Shared->Barrier);
	Result = (u8*)(WaveInfo->Shared->Arena->Start + WaveInfo->Shared->Arena->At);
	MemoryCopy(Bytes, Result, ByteCount);
	WaveBarrier(WaveInfo->Shared->Barrier);
}

#define BroadcastToAllLanes(WaveInfo, Variable) BroadcastBytes(WaveInfo, &Variable, sizeof(Variable))

s64 WaveGetLaneIndex(wave_info *WaveInfo)
{
	return WaveInfo->Lane;
}

u64 NearestPowerOfTwo(u64 Value)
{
	if(Value == 0) return 1;
	Value -= 1;
	Value |= Value >> 1;
	Value |= Value >> 2;
	Value |= Value >> 4;
	Value |= Value >> 8;
	Value |= Value >> 16;
	Value |= Value >> 32;
	return Value + 1;
}

/*
This could be faster probably but it's fine and works,
I don't own a threadripper so this will be fine for
32 core computers...

The following link shows a faster version that would work better.

GPU Gems 3
https://www.youtube.com/watch?v=1G8CZioSjnM
*/
u64 InclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = Value;
	WaveInfo->Shared->Data[0] = 0;
	WaveBarrier(WaveInfo->Shared->Barrier);

	int PerLaneSum = 0;
	for(int I = 0; I <= WaveInfo->Lane; I++)
	{
		PerLaneSum += WaveInfo->Shared->Data[WaveInfo->Lane];
	}
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = PerLaneSum;
	WaveBarrier(WaveInfo->Shared->Barrier);
	return WaveInfo->Shared->Data[WaveInfo->Lane];
}

u64 ExclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = Value;
	WaveInfo->Shared->Data[0] = 0;
	WaveBarrier(WaveInfo->Shared->Barrier);

	int PerLaneSum = 0;
	for(int I = 0; I <= WaveInfo->Lane; I++)
	{
		PerLaneSum += WaveInfo->Shared->Data[WaveInfo->Lane];
	}
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = PerLaneSum;
	WaveBarrier(WaveInfo->Shared->Barrier);
	return WaveInfo->Shared->Data[WaveInfo->Lane];
}

u64 WaveSum(wave_info *WaveInfo, u64 Value)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_store_explicit(WaveInfo->Shared->Data + 0, 0, memory_order_release);
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_fetch_add_explicit(WaveInfo->Shared->Data + 0, Value, memory_order_consume);
	WaveBarrier(WaveInfo->Shared->Barrier);
	return atomic_load_explicit(WaveInfo->Shared->Data + 0, memory_order_acquire);
}

b32 WaveAnyTrue(wave_info *WaveInfo, b32 Expression)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_store_explicit(WaveInfo->Shared->Data + 0, 0, memory_order_release);
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_fetch_add_explicit(WaveInfo->Shared->Data + 0, Expression, memory_order_consume);
	WaveBarrier(WaveInfo->Shared->Barrier);
	return atomic_load_explicit(WaveInfo->Shared->Data + 0, memory_order_acquire) > 0;
}

b32 WaveAllTrue(wave_info *WaveInfo, b32 Expression)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_store_explicit(WaveInfo->Shared->Data + 0, 0, memory_order_release);
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_fetch_add_explicit(WaveInfo->Shared->Data + 0, Expression, memory_order_acq_rel);
	WaveBarrier(WaveInfo->Shared->Barrier);
	return atomic_load_explicit(WaveInfo->Shared->Data + 0, memory_order_acquire) == WaveInfo->Shared->LaneCount;
}