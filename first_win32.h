#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>

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

#define MemoryCopy(Destination, Source, NumberOfBytes) memcpy(Destination, Source, NumberOfBytes)

#define KiloBytes(X) (1024ll*X)
#define MegaBytes(X) (1024ll*KiloBytes(1))
#define GigaBytes(X) (1024ll*MegaBytes(1))

#define Str8ToPrintfArgs(String) (int)String.Size, String.Bytes
#define Str8(String) (str8){strlen(String), String}

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
    str8 OutputPath;

    str8 WorkingDirectory;

    str8_list IncludePaths;
    str8_list SourceFiles;
    
    b32 Optimise;
    b32 GenerateDebugInfo;
}
build_options;

u64 GetPageSize()
{
    SYSTEM_INFO SystemInfo = {0};
    GetSystemInfo(&SystemInfo);

    return SystemInfo.dwPageSize;
}

u8* ArenaPush(memory_arena *Arena, s64 Bytes)
{
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

u8* ArenaPushZero(memory_arena *Arena, s64 Bytes)
{
    u8* Memory = ArenaPush(Arena, Bytes);
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

char* CStrFromStr8(memory_arena *Arena, str8 String)
{
    s32 NullTerminated = 0;
    if(String.Bytes[String.Size] == 0)
    {
        NullTerminated = 1;
    }
   
    char* Result = (char*)ArenaPushZero(Arena, String.Size + NullTerminated);
    MemoryCopy(Result, String.Bytes, String.Size);
    return Result;
}

b32 Str8Equal(str8 A, str8 B)
{
    if(A.Size != B.Size) return 0;
    for(u64 Index = 0; Index < A.Size; Index++)
    {
        if(A.Bytes[Index] != B.Bytes[Index])
        {
            return 0;
        }
    }
    return 1;
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

void Str8ListInsertBefore(memory_arena *Arena, str8_list *List, str8_list_node* Node, str8 String)
{
    if(Node->Previous)
    {
        str8_list_node *Insert = ArenaPushTypeZero(Arena, str8_list_node);
        Insert->String = Str8PushF(Arena, "%.*s", Str8ToPrintfArgs(String));
        Insert->Next = Node;
        Insert->Previous = Node->Previous;
        Node->Previous = Insert;
        Node->Previous->Next = Insert;
    }
    else
    {
        Str8ListPrepend(Arena, List, String);
    }
}

void Str8ListInsertAfter(memory_arena *Arena, str8_list *List, str8_list_node* Node, str8 String)
{
    if(Node->Next)
    {
        str8_list_node *Insert = ArenaPushTypeZero(Arena, str8_list_node);
        Insert->String = Str8PushF(Arena, "%.*s", Str8ToPrintfArgs(String));
        Insert->Previous = Node;
        Insert->Next = Node->Next;
        Node->Next = Insert;
        Node->Next->Previous = Insert;
    }
    else
    {
        Str8ListAppend(Arena, List, String);
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

str8 Str8ListJoin(memory_arena *Arena, str8_list *List)
{
    u64 BytesToAllocate = 0;
    for(str8_list_node *Node = List->First; Node != 0; Node = Node->Next)
    {
        BytesToAllocate += Node->String.Size;
    }
    u8* Bytes = ArenaPush(Arena, BytesToAllocate);
    u64 At = 0;
    for(str8_list_node *Node = List->First; Node != 0; Node = Node->Next)
    {
        MemoryCopy(Bytes + At, Node->String.Bytes, Node->String.Size);
        At += Node->String.Size;
    }
    str8 Result = { .Bytes = Bytes, .Size = BytesToAllocate };

    return Result;
}

str8 LoadEntireFileIntoMemory(memory_arena *Arena, str8 Filename)
{
    HANDLE FileHandle = CreateFileA(CStrFromStr8(Arena, Filename),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    0,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    0);

    if (FileHandle == INVALID_HANDLE_VALUE) 
    {
        printf("File not found");
        return (str8){0};
    }

    LARGE_INTEGER FileSize;
    DWORD BytesRead;
    GetFileSizeEx(FileHandle, &FileSize);

    u8* BackingMemory = ArenaPush(Arena, (u64)FileSize.QuadPart+1);
    BackingMemory[FileSize.QuadPart] = 0;

    str8 Result = {0};
    Result.Bytes = BackingMemory;
    Result.Size = FileSize.QuadPart;

    OVERLAPPED Overlapped = {0};
    ReadFile(FileHandle, BackingMemory, FileSize.QuadPart, &BytesRead, &Overlapped);

    CloseHandle(FileHandle);

    return Result;
}

str8 GetExecutableDirectory(memory_arena *Arena)
{
    DWORD NumberOfBytes = GetCurrentDirectoryA(0, 0);
    u8* BackingMemory = ArenaPushZero(Arena, NumberOfBytes);

    GetCurrentDirectoryA(NumberOfBytes, (LPSTR)BackingMemory);

    str8 Result = {0};
    Result.Size = NumberOfBytes;
    Result.Bytes = BackingMemory;

    return Result;
}

#define FNV_64_PRIME ((u64)0x100000001b3ULL)
#define FNV_HASH_START ((u64)0xcbf29ce484222325ULL)
void FNVHash64(u64 *Hash, str8 String)
{
    for(u64 Index = 0; Index < String.Size; Index++)
    {

        *Hash *= FNV_64_PRIME;
        *Hash ^= (u64)String.Bytes[Index];
    }

}

b32 Advance(str8 File, u64 *At)
{
    if(*At < File.Size)
    {
        *At += 1;
        return 1;
    }
    return 0;
}

b32 AdvanceN(str8 File, u64 *At, u64 N)
{
    if(*At + N < File.Size)
    {
        *At += N;
        return 1;
    }
    return 0;
}

void GobbleWhitespace(str8 File, u64 *At)
{
    while(*At < File.Size)
    {
        u64 Byte = File.Bytes[*At];
        switch(Byte)
        {
            case ' ':
            case '\r':
            case '\n':
            {
                Advance(File, At);
            } break;
            default: {
                return;
            } break;
        }
    }
}

str8 Str8UpToLastForwardSlash(memory_arena *Arena, str8 InputPath)
{
    u64 SizeUptoLastForwardSlash = InputPath.Size - 1;
    b32 FoundSlash = 0;
    for(; SizeUptoLastForwardSlash >= 0; SizeUptoLastForwardSlash--)
    {
        if(InputPath.Bytes[SizeUptoLastForwardSlash] == '/')
        {
            FoundSlash = 1;
            break;
        }
    }
    str8 Path = {0};
    if(FoundSlash)
    {
        Path.Size = SizeUptoLastForwardSlash;
        Path.Bytes = ArenaPush(Arena, SizeUptoLastForwardSlash);
        MemoryCopy(Path.Bytes, InputPath.Bytes, SizeUptoLastForwardSlash);
    }
    return Path;
}

str8 Str8FromLastForwardSlash(memory_arena *Arena, str8 InputPath)
{
    u64 SizeUptoLastForwardSlash = InputPath.Size - 1;
    b32 FoundSlash = 0;
    for(; SizeUptoLastForwardSlash >= 0; SizeUptoLastForwardSlash--)
    {
        if(InputPath.Bytes[SizeUptoLastForwardSlash] == '/')
        {
            FoundSlash = 1;
            break;
        }
    }
    str8 Path = {0};
    u64 SizeFromLastForwardSlash = InputPath.Size - SizeUptoLastForwardSlash -1;
    if(FoundSlash && SizeFromLastForwardSlash >= 0)
    {
        Path.Size = SizeFromLastForwardSlash;
        Path.Bytes = ArenaPush(Arena, SizeFromLastForwardSlash);
        MemoryCopy(Path.Bytes, InputPath.Bytes + SizeUptoLastForwardSlash + 1, SizeFromLastForwardSlash);
    }
    return Path;
}

str8 Str8FromLastFullstop(memory_arena *Arena, str8 InputPath)
{
    u64 SizeUptoLastFullstop = InputPath.Size - 1;
    b32 FoundSlash = 0;
    for(; SizeUptoLastFullstop >= 0; SizeUptoLastFullstop--)
    {
        if(InputPath.Bytes[SizeUptoLastFullstop] == '.')
        {
            FoundSlash = 1;
            break;
        }
    }
    str8 Path = {0};
    u64 SizeFromLastFullstop = InputPath.Size - SizeUptoLastFullstop -1;
    if(FoundSlash && SizeFromLastFullstop >= 0)
    {
        Path.Size = SizeFromLastFullstop;
        Path.Bytes = ArenaPush(Arena, SizeFromLastFullstop);
        MemoryCopy(Path.Bytes, InputPath.Bytes + SizeUptoLastFullstop + 1, SizeFromLastFullstop);
    }
    return Path;
}

void FindAllIncludesInFile(memory_arena *Arena, str8 Filename, str8_list IncludePaths, str8_list *IncludeChainFilenames, str8_list *IncludeChainFiles)
{
    // Support unicode...

    str8 CurrentFile = LoadEntireFileIntoMemory(Arena, Filename);
    Str8ListAppend(Arena, IncludeChainFiles, CurrentFile);

    for(u64 At = 0; At < CurrentFile.Size; At++)
    {
        u8 Byte = CurrentFile.Bytes[At];
        if(Byte == '#')
        {
            if(Advance(CurrentFile, &At) == 0) return;
            str8 Token = {0};
            Token.Bytes = CurrentFile.Bytes + At;
            str8 IncludeToken = Str8("include");
            Token.Size = IncludeToken.Size;
            if(At + Token.Size < CurrentFile.Size)
            {
                if(Str8Equal(Token, IncludeToken))
                {
                    AdvanceN(CurrentFile, &At, Token.Size);
                    GobbleWhitespace(CurrentFile, &At);
                    if(CurrentFile.Bytes[At] == '"')
                    {
                        if(Advance(CurrentFile, &At))
                        {
                            str8 IncludePath = {0};
                            IncludePath.Bytes = CurrentFile.Bytes + At;
                            while(Advance(CurrentFile, &At) && CurrentFile.Bytes[At] != '"');
                            IncludePath.Size = CurrentFile.Bytes + At - IncludePath.Bytes;
                            
                            b32 FoundFile = 0;

                            DWORD FileAttributes = GetFileAttributesA((LPCSTR)CStrFromStr8(Arena, IncludePath));
                            if(FileAttributes != INVALID_FILE_ATTRIBUTES && !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                            {
                                FoundFile = 1;
                                Str8ListAppend(Arena, IncludeChainFilenames, IncludePath);
                                FindAllIncludesInFile(Arena, IncludePath, IncludePaths, IncludeChainFilenames, IncludeChainFiles);
                            }
                            else
                            {
                                for(str8_list_node *Node = IncludePaths.First; Node != 0; Node = Node->Next)
                                {
                                    str8 FullPath = Str8PushF(
                                        Arena, 
                                        "%.*s/%.*s", 
                                        Str8ToPrintfArgs(Str8UpToLastForwardSlash(Arena, Node->String)),
                                        Str8ToPrintfArgs(IncludePath)
                                    );
                                    DWORD FileAttributes = GetFileAttributesA((LPCSTR)CStrFromStr8(Arena, FullPath));
                                    if(FileAttributes != INVALID_FILE_ATTRIBUTES && !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                                    {
                                        FoundFile = 1;
                                        Str8ListAppend(Arena, IncludeChainFilenames, FullPath);
                                        FindAllIncludesInFile(Arena, FullPath, IncludePaths, IncludeChainFilenames, IncludeChainFiles);
                                        break;
                                    }
                                }
                            }

                            if(FoundFile == 0)
                            {
                                printf("\nUnable to find include path...\n");
                            }
                        }
                    }
                }
            }
        }
    }
}



void CompileFromBuildOptions(memory_arena* Arena, build_options Options)
{
    STARTUPINFO StartupInfo = {0};
    PROCESS_INFORMATION ProcessInfo = {0};

    SECURITY_ATTRIBUTES SecurityAttributes;
    SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecurityAttributes.bInheritHandle = 1;
    SecurityAttributes.lpSecurityDescriptor = 0;

    HANDLE StdOutHandleRead = 0;
    HANDLE StdOutHandleWrite = 0;

    if(CreatePipe(&StdOutHandleRead, &StdOutHandleWrite, &SecurityAttributes, 0) == 0)
    {
        printf("Failed to create stdout pipe");
    }
    SetHandleInformation( StdOutHandleRead, HANDLE_FLAG_INHERIT, 0);

    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdError = StdOutHandleWrite;
    StartupInfo.hStdOutput = StdOutHandleWrite;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;


    LARGE_INTEGER Start = {0};
    QueryPerformanceCounter(&Start);

    str8 Compilers[COMPILER_COUNT] = { Str8("cl.exe"), Str8("clang.exe") };
    str8 Extensions[OUTPUT_COUNT] = { Str8(".exe"), Str8(".lib"), Str8(".dll") };
    for(str8_list_node *Node = Options.IncludePaths.First; Node != 0; Node = Node->Next)
    {
        Str8ListInsertBefore(Arena, &Options.IncludePaths, Node, Str8("/I"));
        Str8ListInsertAfter(Arena, &Options.IncludePaths, Node, Str8(" "));
        Node = Node->Next;
    }

    printf("Compiling...\n");
    str8_list HashedFilenames = {0};
    for(str8_list_node *Node = Options.SourceFiles.First; Node != 0; Node = Node->Next)
    {
        str8_list IncludeChainFiles = {0};
        str8_list IncludeChainFilenames = {0};
        
        FindAllIncludesInFile(Arena, Node->String, Options.IncludePaths, &IncludeChainFilenames, &IncludeChainFiles);
        u64 Hash = FNV_HASH_START;
        FNVHash64(&Hash, Node->String);
        for(str8_list_node *File = IncludeChainFiles.First; File != 0; File = File->Next)
        {
            FNVHash64(&Hash, File->String);
        }

        str8 HashedFilename = Str8PushF(Arena, "%llu.o", Hash);
        Str8ListAppend(Arena, &HashedFilenames, HashedFilename);

        DWORD FileAttributes = GetFileAttributesA((LPCSTR)CStrFromStr8(Arena, Str8PushF(Arena, "build/%.*s", Str8ToPrintfArgs(HashedFilename))));
        if(FileAttributes != INVALID_FILE_ATTRIBUTES && !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            continue;
        }

        printf("Rebuilding TU: %.*s\n", Str8ToPrintfArgs(Node->String));

        str8 CompileCommand = Str8PushF(
                        Arena, 
                        "%.*s /c /nologo /Fo%s%.*s %.*s /EHsc %.*s",
                        Str8ToPrintfArgs(Compilers[Options.Compiler]),
                        "build/",
                        Str8ToPrintfArgs(HashedFilename),
                        Str8ToPrintfArgs(Str8ListJoin(Arena, &Options.IncludePaths)),
                        Str8ToPrintfArgs(Node->String)
                    );

        DWORD ExitCode;
        if(CreateProcessA(0, CompileCommand.Bytes, 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInfo))
        {
            WaitForSingleObject(ProcessInfo.hProcess, INFINITE);

            GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
            CloseHandle(ProcessInfo.hProcess);
            CloseHandle(ProcessInfo.hThread);
            CloseHandle(StdOutHandleWrite);
        }
        else
        {
            printf("Failed to launch compiler... if using cl.exe try using the x64 command prompt\n");
            return;
        }

        if(ExitCode)
        {
            printf("\nFailed to Compile\n\n");
            DWORD NumberOfBytesRead = 0;
            str8_list StdOutList = {0};
            for (;;)
            {
                u8* BackingMemory = ArenaPushZero(Arena, 4096);
                BOOL Success = ReadFile(StdOutHandleRead, BackingMemory, 4096, &NumberOfBytesRead, NULL);
                if (!Success || NumberOfBytesRead == 0)
                {
                    break;
                }
                str8 String = {.Bytes = BackingMemory, .Size = NumberOfBytesRead };
                Str8ListAppend(Arena, &StdOutList, String);
            }
            str8 OutputString = Str8ListJoin(Arena, &StdOutList);
            printf("%.*s", Str8ToPrintfArgs(OutputString));
            return;
        }
    }

    str8_list ObjectFiles = {0};

    for(str8_list_node *Node = HashedFilenames.First; Node != 0; Node = Node->Next)
    {
        Str8ListAppend(Arena, &ObjectFiles, Str8PushF(Arena, "%.*s%.*s ", Str8ToPrintfArgs(Options.OutputPath), Str8ToPrintfArgs(Node->String)));
    }


    WIN32_FIND_DATA FindData = {0};
    str8 ObjectFilesInBuildDirectory = Str8PushF(Arena, "%.*s*.o", Str8ToPrintfArgs(Options.OutputPath));
    printf("%s\n", ObjectFilesInBuildDirectory.Bytes);
    HANDLE FindHandle = FindFirstFile(ObjectFilesInBuildDirectory.Bytes, &FindData);

    b32 NextFile = FindHandle != INVALID_HANDLE_VALUE;
    for(;NextFile != 0; NextFile = FindNextFile(FindHandle, &FindData))
    {
        if(FindData.dwFileAttributes != INVALID_FILE_ATTRIBUTES && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            str8 FoundFilename = Str8PushF(Arena, "%s", FindData.cFileName);
            b32 FoundMatch = 0;
            for(str8_list_node *Node = HashedFilenames.First; Node != 0; Node = Node->Next)
            {
                if(Str8Equal(Node->String, FoundFilename))
                {
                    printf("%.*s\n", Str8ToPrintfArgs(FoundFilename));
                    FoundMatch = 1;
                    break;
                }
            }
            if(FoundMatch == 0)
            {
                str8 S = Str8PushF(Arena, "%.*s%s", Str8ToPrintfArgs(Options.OutputPath), FindData.cFileName);
                printf("ooga booga %s", S.Bytes);
                DeleteFile(S.Bytes);
            }
        }
    }

    str8 LinkCommand = Str8PushF(
                Arena, 
                "%.*s /nologo /Fe%.*s%.*s %.*s /EHsc",
                Str8ToPrintfArgs(Compilers[Options.Compiler]),
                Str8ToPrintfArgs(Options.OutputPath),
                Str8ToPrintfArgs(Options.OutputName),
                Str8ToPrintfArgs(Str8ListJoin(Arena, &ObjectFiles))
            );
    printf("Link Command: %.*s\n", Str8ToPrintfArgs(LinkCommand));

    DWORD ExitCode;
    if(CreateProcessA(0, LinkCommand.Bytes, 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInfo))
    {
        WaitForSingleObject(ProcessInfo.hProcess, INFINITE);

        GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
        CloseHandle(ProcessInfo.hProcess);
        CloseHandle(ProcessInfo.hThread);
        CloseHandle(StdOutHandleWrite);
    }
    else
    {
        printf("Failed to launch linker... if using cl.exe try using the x64 command prompt\n");
        return;
    }

    if(ExitCode)
    {
        printf("\nFailed to Link\n\n");
        DWORD NumberOfBytesRead = 0;
        str8_list StdOutList = {0};
        for (;;)
        {
            u8* BackingMemory = ArenaPushZero(Arena, 4096);
            BOOL Success = ReadFile(StdOutHandleRead, BackingMemory, 4096, &NumberOfBytesRead, NULL);
            if (!Success || NumberOfBytesRead == 0)
            {
                break;
            }
            str8 String = {.Bytes = BackingMemory, .Size = NumberOfBytesRead };
            Str8ListAppend(Arena, &StdOutList, String);
        }
        str8 OutputString = Str8ListJoin(Arena, &StdOutList);
        printf("%.*s", Str8ToPrintfArgs(OutputString));
        return;
    }


    LARGE_INTEGER End = {0};
    LARGE_INTEGER Frequency = {0};
    QueryPerformanceCounter(&End);
    QueryPerformanceFrequency(&Frequency);

    f64 BuildTimeInMilliseconds = (f64)(1000.0*(End.QuadPart - Start.QuadPart) / Frequency.QuadPart);

    printf("Finished Compiling\n\n");
    printf("Build took %0.2fms\n", BuildTimeInMilliseconds);

   CloseHandle(StdOutHandleRead);
}