#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef float              f32;
typedef double             f64;
typedef unsigned char      b8;
typedef unsigned char      b16;
typedef unsigned int       b32;
typedef unsigned long long b64;

#define MemoryAllocate(NumberOfBytes) (u8*)VirtualAlloc((void*)0, NumberOfBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
#define MemoryCommit(Address, NumberOfBytes) VirtualAlloc((void*)(Address), NumberOfBytes, MEM_COMMIT, PAGE_READWRITE)
#define MemoryReserve(NumberOfBytes) (u8*)VirtualAlloc((void*)0, NumberOfBytes, MEM_RESERVE, PAGE_READWRITE)

#define KiloBytes(X) (1024ll*X)
#define MegaBytes(X) (1024ll*KiloBytes(1))
#define GigaBytes(X) (1024ll*MegaBytes(1))

typedef struct memory_arena {
    u8* Start;
    s64 Committed;
    s64 At;
} memory_arena;

typedef struct str8 {
    s64 Size;
    u8* Bytes;
} str8;

typedef struct str8_list {
    str8* First;
    str8* Last;
} str8_list;

u64 GetPageSize() {
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    return SystemInfo.dwPageSize;
}

u8* ArenaPush(memory_arena* Arena, s64 Bytes) {
    if(Arena->At + Bytes > Arena->Committed) {
        u64 BytesNeeded = (Arena->At + Bytes - Arena->Committed);
        u64 BytesToCommit = (BytesNeeded + GetPageSize() - 1)/GetPageSize();
        MemoryCommit(Arena->Start + Arena->Committed, BytesToCommit);
        Arena->Committed += BytesToCommit;
    }
    u8* Memory = Arena->Start + Arena->At;
    Arena->At += Bytes;
    
    return Memory;
}