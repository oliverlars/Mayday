#include <windows.h>
#include <assert.h>
#include <stdatomic.h>
#include <winnt.h>

#include "wave.h"
#include "memory.h"
#include "log.h"

s64 AtomicAdd(s64 *Value, s64 Addend)
{
	return InterlockedAdd64(Value, Addend);
}

void WaveBarrier(RTL_BARRIER *Barrier)
{
	EnterSynchronizationBarrier(Barrier, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY);
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


void *DispatchWorker(void *Data)
{
	wave_info *WaveInfo = Data;
	shared_wave_info *Shared = WaveInfo->Shared;

	logger Logger = {0};
	Logger.Arena = ArenaInitialise(GigaBytes(1));
	per_thread_context *ThreadContext = GetThreadLocalContext();
	ThreadContext->Logger = &Logger;
	ThreadContext->ScratchArenas[0] = ArenaInitialise(GigaBytes(1));
	ThreadContext->ScratchArenas[1] = ArenaInitialise(GigaBytes(1));

	while(atomic_load_explicit(&Shared->WorkAvailable, memory_order_acquire) == 0)
	{
		atomic_int Expected = 0;
		WaitOnAddress(&Shared->WorkAvailable, &Expected, sizeof(atomic_int), INFINITE);
	}

	for(;;)
	{
		//while(!Shared->Entry || !Shared->EntryData);
		Shared->Entry(WaveInfo, Shared->EntryData);
		atomic_fetch_sub_explicit(&Shared->WorkAvailable, 1, memory_order_release);
		WaveBarrier(Shared->TaskBarrier);

		while(atomic_load_explicit(&Shared->WorkAvailable, memory_order_acquire) == 0)
		{
			atomic_int Expected = 0;
			WaitOnAddress(&Shared->WorkAvailable, &Expected, sizeof(atomic_int), INFINITE);
			
		}
	}
}

void DispatchInitialise(shared_wave_info *Shared, s64 LaneCount)
{

	wave_info *Entries = (wave_info*)ArenaPush(Shared->Arena, LaneCount*sizeof(wave_info));
	HANDLE *Waves = (HANDLE*)ArenaPush(Shared->Arena, LaneCount*sizeof(HANDLE));
	
	barrier *Barrier = ArenaPushArrayZero(Shared->Arena, barrier, 1);

	barrier *TaskBarrier = ArenaPushArrayZero(Shared->Arena, barrier, 1);
	InitializeSynchronizationBarrier(Barrier, LaneCount, -1);
	Shared->LaneCount = LaneCount;
	for (s64 Lane = 0; Lane < LaneCount; Lane++)
	{
		Entries[Lane].Shared = Shared;
		Entries[Lane].Lane = Lane;
		Entries[Lane].Shared->Barrier = Barrier;
		Entries[Lane].Shared->TaskBarrier = TaskBarrier;
		DWORD ThreadID;
		Waves[Lane] = CreateThread(0, 0, Null, Entries + Lane, 0, &ThreadID);
	}
}

void Dispatch(shared_wave_info *Shared, void (*Entry)(wave_info *, void*), void* EntryData)
{
	
	Shared->EntryData = EntryData;
	Shared->Entry = Entry;
	atomic_store_explicit(&Shared->WorkAvailable, Shared->LaneCount, memory_order_release);

	WakeByAddressAll(&Shared->WorkAvailable);

	WaveBarrier(Shared->TaskBarrier);

	while(atomic_load_explicit(&Shared->WorkAvailable, memory_order_acquire) > 0);
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
	Result = (u8*)WaveSum(WaveInfo, (u64)Result);
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
u64 ExclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = Value;
	WaveBarrier(WaveInfo->Shared->Barrier);

	u64 PerLaneSum = 0;
	for(int I = 0; I < WaveInfo->Lane; I++)
	{
		PerLaneSum += WaveInfo->Shared->Data[I];
	}
	WaveBarrier(WaveInfo->Shared->Barrier);
	return PerLaneSum;
}

u64 InclusiveWavePrefixSum(wave_info *WaveInfo, u64 Value)
{
	WaveBarrier(WaveInfo->Shared->Barrier);
	WaveInfo->Shared->Data[WaveInfo->Lane] = Value;
	WaveBarrier(WaveInfo->Shared->Barrier);

	int PerLaneSum = 0;
	for(int I = 0; I <= WaveInfo->Lane; I++)
	{
		PerLaneSum += WaveInfo->Shared->Data[I];
	}

	WaveBarrier(WaveInfo->Shared->Barrier);
	return PerLaneSum;
}

u64 WaveSum(wave_info *WaveInfo, u64 Value)
{
	LogInfo("Lane: ?u32", WaveInfo->Lane);
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_store_explicit(WaveInfo->Shared->Data + 0, 0, memory_order_release);
	WaveBarrier(WaveInfo->Shared->Barrier);
	atomic_fetch_add_explicit(WaveInfo->Shared->Data + 0, 1, memory_order_seq_cst);
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