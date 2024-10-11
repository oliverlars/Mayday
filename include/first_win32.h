#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>

#include <stdio.h>

#include <stdint.h>

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef float     f32;
typedef double    f64;
typedef uint8_t   b8;
typedef uint16_t  b16;
typedef uint32_t  b32;
typedef uint64_t  b64;

#define MemoryAllocate(NumberOfBytes) (u8*)VirtualAlloc((void*)0, NumberOfBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
#define MemoryCommit(Address, NumberOfBytes) VirtualAlloc((void*)(Address), NumberOfBytes, MEM_COMMIT, PAGE_READWRITE)
#define MemoryReserve(NumberOfBytes) (u8*)VirtualAlloc((void*)0, NumberOfBytes, MEM_RESERVE, PAGE_READWRITE)

#define KiloBytes(X) (1024ll*X)
#define MegaBytes(X) (1024ll*KiloBytes(1))
#define GigaBytes(X) (1024ll*MegaBytes(1))

#define Str8ToPrintfArgs(String) (int)String.Size, String.Bytes
#define Str8Lit(String) (str8){strlen(String), String}

#define ArenaPushType(Arena, Type) (Type*)ArenaPush(Arena, sizeof(Type))
#define ArenaPushTypeZero(Arena, Type) (Type*)ArenaPushZero(Arena, sizeof(Type))

typedef struct memory_arena
{
    u8 *Start;
    s64 Committed;
    s64 At;
}
memory_arena;

typedef struct str8
{
    s64 Size;
    u8 *Bytes;
}
str8;

typedef struct str8_list_node
{
    struct str8_list_node *Next;
    struct str8_list_node *Previous;
    str8 String;
}
str8_list_node;

typedef struct str8_list
{
    str8_list_node *First;
    str8_list_node *Last;
}
str8_list;

typedef enum compiler
{
    COMPILER_MSVC,
    COMPILER_CLANG,
    COMPILER_COUNT,
}
compiler;

typedef enum output_type
{
    OUTPUT_EXECUTABLE,
    OUTPUT_STATIC_LIBRARY,
    OUTPUT_DYNAMIC_LIBRARY,
    OUTPUT_COUNT,
}
output_type;

typedef struct build_options
{
    compiler Compiler;
    output_type OutputType;
    str8 OutputName;
    str8_list IncludePaths;
    str8_list SourceFiles;
}
build_options;

u64 GetPageSize()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    return SystemInfo.dwPageSize;
}

u8* ArenaPush(memory_arena *Arena, s64 Bytes)
{
    if(Arena->At + Bytes > Arena->Committed)
    {
        u64 BytesNeeded = (Arena->At + Bytes - Arena->Committed);
        u64 BytesToCommit = (BytesNeeded + GetPageSize() - 1)/GetPageSize();
        MemoryCommit(Arena->Start + Arena->Committed, BytesToCommit);
        Arena->Committed += BytesToCommit;
    }
    u8* Memory = Arena->Start + Arena->At;
    Arena->At += Bytes;
    
    return Memory;
}

u8* ArenaPushZero(memory_arena *Arena, s64 Bytes)
{
    if(Arena->At + Bytes > Arena->Committed)
    {
        u64 BytesNeeded = (Arena->At + Bytes - Arena->Committed);
        u64 BytesToCommit = (BytesNeeded + GetPageSize() - 1)/GetPageSize();
        MemoryCommit(Arena->Start + Arena->Committed, BytesToCommit);
        Arena->Committed += BytesToCommit;
    }
    u8* Memory = Arena->Start + Arena->At;
    Arena->At += Bytes;
    
    ZeroMemory(Memory, Bytes);
    return Memory;
}


str8 Str8PushFV(memory_arena *Arena, char *Format, va_list InputArgList)
{
    va_list ArgList;
    va_copy(ArgList, InputArgList);
    u32 BytesNeeded = vsnprintf(0, 0, Format, InputArgList) + 1;
    str8 Result = {0};
    Result.Bytes = ArenaPush(Arena, BytesNeeded);
    Result.Size = vsnprintf((char*)Result.Bytes, BytesNeeded, Format, ArgList);
    Result.Bytes[Result.Size] = 0;
    va_end(ArgList);
    return Result;
}

str8 Str8PushF(memory_arena *Arena, char *Format, ...)
{
    va_list args;
    va_start(args, Format);
    str8 Result = Str8PushFV(Arena, Format, args);
    va_end(args);
    return Result;
}

void Str8ListAppend(memory_arena *Arena, str8_list *List, str8 String)
{
    str8_list_node *Node = ArenaPushTypeZero(Arena, str8_list_node);
    Node->String = Str8PushF(Arena, "%.*s", Str8ToPrintfArgs(String));
    if(List->First == 0)
    {
        List->First = Node;
        List->Last = List->First;
    }
    else
    {
        List->Last->Next = Node;
        Node->Previous = List->Last;
        List->Last = Node;
    }
}

void Str8ListPrepend(memory_arena *Arena, str8_list *List, str8 String)
{
    str8_list_node *Node = ArenaPushTypeZero(Arena, str8_list_node);
    Node->String = Str8PushF(Arena, "%.*s", Str8ToPrintfArgs(String));
    if(List->First == 0)
    {
        List->First = Node;
        List->Last = List->First;
    }
    else
    {
        List->First->Previous = Node;
        Node->Next = List->First;
        List->First = Node;
    }
}

void Str8ListPopN(str8_list *List, u64 N)
{
    str8_list_node *Node = List->First;
    for(int Index = 0; Index < N; Index++, Node = Node->Next);
    if(Node != 0)
    {
        if(Node->Previous != 0)
        {
            Node->Previous->Next = Node->Next;
        }
        else
        {
            List->First = List->First->Next;
        }

        if(Node->Next != 0)
        {
            Node->Next->Previous = Node->Previous;
        }
        else
        {
            List->Last = List->Last->Previous;
        }
    }
}

void Str8ListPopLast(str8_list *List) {
    if(List->Last)
    {
        if(List->First == List->Last)
        {
            List->First = 0;
            List->Last = 0;
        }
        else
        {
            List->Last = List->Last->Previous;
        }
    }
}

void Str8ListPopFirst(str8_list *List) {
    if(List->First)
    {
        if(List->First == List->Last)
        {
            List->First = 0;
            List->Last = 0;
        }
        else
        {
            List->First = List->First->Next;
        }
    }
}

void CompileFromBuildOptions(build_options Options)
{
    STARTUPINFO StartupInfo = {0};
    PROCESS_INFORMATION ProcessInfo = {0};

    SECURITY_ATTRIBUTES SecurityAttributes;
    SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecurityAttributes.bInheritHandle = 1;
    SecurityAttributes.lpSecurityDescriptor = 0;

    HANDLE StdInHandle = 0;
    HANDLE StdOutHandle = 0;

    CreatePipe(&StdOutHandle, &StdInHandle, &SecurityAttributes, 0);

    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdError = StdOutHandle;
    StartupInfo.hStdOutput = StdOutHandle;
    StartupInfo.hStdInput = StdInHandle;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    str8 Compilers[2] = {Str8Lit("cl.exe"), Str8Lit("clang.exe")};
    CreateProcessA(Compilers[Options.Compiler].Bytes, "src/mayday.c /link /OUT:mayday.exe", 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInfo);
}