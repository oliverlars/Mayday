#include "log.h"
#include "memory.h"
#include <assert.h>
#include <errno.h>
#define SOKOL_NO_ENTRY
#define SOKOL_D3D11
#define SOKOL_DEBUG
#include "sokol_app.h"

#include "strings.h"
#include "os.h"
#include <windows.h>

#include <string.h>
#include <stdio.h>

#include <time.h>

operating_system OS_GetOperatingSystem()
{
	return OS_WINDOWS;
}

os_window OS_MakeWindow(str8 Name, int Width, int Height)
{
	os_window Window = {0};
	Window.Width = Width;
	Window.Height = Height;
	Window.Title = Name;

	return Window;
}

void OS_Run(os_window Window, void *Data)
{
	sapp_desc SAPPDesc = (sapp_desc)
	{
		.window_title = "assdfasdfsd",
		.init_userdata_cb = OS_Initialise,
		.frame_userdata_cb = OS_Update,
		.event_userdata_cb = OS_Event,
		.width = Window.Width,
		.height = Window.Height,
		.user_data = Data,
		.high_dpi = false,
		.cleanup_userdata_cb = OS_Exit,
	};
	sapp_run(&SAPPDesc);
}

os_handle OS_GetWindowHandle()
{
	return (os_handle)sapp_win32_get_hwnd();
}

int OS_WindowWidth(os_window *Window)
{
	return sapp_width();
}
int OS_WindowHeight(os_window *Window)
{
	return sapp_height();
}


u64 OS_TimeStampNow()
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return (u64)Counter.QuadPart;
}

f32 OS_MilliSecondsFromTimeStamp(u64 TimeStamp)
{
    LARGE_INTEGER Freq = {0};
    QueryPerformanceFrequency(&Freq);

    return (f32)(TimeStamp * 1000.0 / (f64)Freq.QuadPart);
}

void AppendDirectoryEntry(m_arena *Arena, os_directory_entry *Entries, os_directory_entry Entry)
{
	if(Entries)
	{
		Entry.Parent = Entries->Parent;
		os_directory_entry *LastEntry = Entries->Parent->Last;

		LastEntry->Next = ArenaPushArray(Arena, os_directory_entry, 1);
		LastEntry->Next[0] = Entry;
		Entries->Parent->Last = LastEntry->Next;
	}
	else
	{
		assert(False);
	}
}

void InsertDirectoryEntryAsChild(m_arena *Arena, os_directory_entry *At, os_directory_entry Entry)
{
	Entry.Parent = At;
	if(At->First)
	{
		AppendDirectoryEntry(Arena, At->First, Entry);
	}
	else
	{
		At->First = ArenaPushArray(Arena, os_directory_entry, 1);
		At->First[0] = Entry;
		At->Last = At->First;
	}
}

b32 OS_IsPathDirectory(str8 Path)
{
	m_scratch_arena Scratch = ScratchBegin(0, 0);
	char *PathCstr = CStrFromStr8(Scratch.Arena, Path);

	DWORD Attributes = GetFileAttributesA(PathCstr);
	b32 Result = False;
	if (Attributes != INVALID_FILE_ATTRIBUTES) {
		Result = ((Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? True : False;
	}

	ScratchEnd(Scratch);
	return Result;
}

s64
OS_DirectoryTreeFromPathHelper(m_arena *Arena,
							   str8         Path,
							   os_directory_entry *Entries)
{
	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);
	str8 Pattern = Str8PushF(Arena, "%S\\*", Path);

	char *PatternCStr = CStrFromStr8(Scratch.Arena, Pattern);
	WIN32_FIND_DATAA FindData;
	HANDLE FindHandle = FindFirstFileA(PatternCStr, &FindData);
	s64    Count = 0;

	if(FindHandle != INVALID_HANDLE_VALUE) {
		do {
			char *Name = FindData.cFileName;
			if(Name[0] != '.' ||
				(Name[1] != '\0' && !(Name[1]=='.' && Name[2]=='\0')))
			{
				os_directory_entry Entry = {0};
				str8 FullPath = Str8PushF(Arena, "%S\\%s", Path, Name);

				if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					Entry.Type        = OS_DIR_ENTRY_FOLDER;
					Entry.PathAndName = FullPath;
					InsertDirectoryEntryAsChild(Arena, Entries, Entry);
					Count += OS_DirectoryTreeFromPathHelper(Arena,
															FullPath,
															Entries->Last);
				}
				else {
					Entry.Type        = OS_DIR_ENTRY_FILE;
					Entry.PathAndName = FullPath;
					InsertDirectoryEntryAsChild(Arena, Entries, Entry);
				}
				Count += 1;
			}
		}
		while(FindNextFileA(FindHandle, &FindData));

		FindClose(FindHandle);
	}
	ScratchEnd(Scratch);
	return Count;
}

os_directory_tree
OS_DirectoryTreeFromPath(m_arena *Arena, str8 Path)
{
	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	os_directory_entry *EntryRoot = ArenaPushArrayZero(Arena,
														os_directory_entry, 1);
	EntryRoot->PathAndName = Str8Copy(Arena, Path);
	EntryRoot->Type        = OS_DIR_ENTRY_FOLDER;

	os_directory_tree Result = {0};

	s64 Count = OS_DirectoryTreeFromPathHelper(Arena,
												Path,
												EntryRoot);
	Result.Count = Count + 1;
	Result.Root  = EntryRoot;

	ScratchEnd(Scratch);
	return Result;
}

b32 OS_ValidPath(str8 Path)
{
	m_scratch_arena Scratch = ScratchBegin(0, 0);
	char *PathCStr = CStrFromStr8(Scratch.Arena, Path);

	DWORD Attributes = GetFileAttributesA(PathCStr);
	b32   Result = (Attributes != INVALID_FILE_ATTRIBUTES) ? True : False;

	ScratchEnd(Scratch);
	return Result;
}

void OS_CountAllEntriesInDirectoryTreeHelper(os_directory_entry *Entry, int *Count)
{
	for(; Entry; Entry = Entry->Next)
	{
		if(Entry->Type == OS_DIR_ENTRY_FOLDER)
		{
			Count[0] = Count[0] + 1;
			OS_CountAllEntriesInDirectoryTreeHelper(Entry->First, Count);
		}
		else
		{
			Count[0] = Count[0] + 1;
		}
	}
}

int OS_CountAllEntriesInDirectoryTree(os_directory_tree Tree)
{
	int Result = 0;
	OS_CountAllEntriesInDirectoryTreeHelper(Tree.Root, &Result);
	return Result;
}

str8 OS_GetLastModifiedTime(m_arena *Arena, str8 Path)
{
	WIN32_FILE_ATTRIBUTE_DATA Attributes;
	if (!GetFileAttributesExA((char*)Path.Bytes,
							  GetFileExInfoStandard,
							  &Attributes))
	{
		return Str8("");
	}
	
	FILETIME LocalFileTime;
	FileTimeToLocalFileTime(&Attributes.ftLastWriteTime, &LocalFileTime);

	SYSTEMTIME SystemTime;
	FileTimeToSystemTime(&LocalFileTime, &SystemTime);

	char *WeekDay[7] = 
	{
		"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
	};

	char *Month[12]=
	{ 
		"Jan","Feb","Mar","Apr","May","Jun",
		"Jul","Aug","Sep","Oct","Nov","Dec"
	};

	char *StringBuffer = ArenaPushArrayZero(Arena, char, 26);
	int Size = _snprintf(StringBuffer, sizeof(StringBuffer), "%s %s %2u %02u:%02u:%02u %u",
						 WeekDay[SystemTime.wDayOfWeek],
						 Month[SystemTime.wMonth-1],
						 SystemTime.wDay,
						 SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
						 SystemTime.wYear);

	str8 Result = {.Bytes = (u8*)StringBuffer, .Size = Size };
	return Result;
}

#define EPOCH_DIFF_100NS 116444736000000000ULL

u64 OS_GetFileLastWriteTimestamp(str8 Path)
{
	m_scratch_arena Scratch = ScratchBegin(0, 0);
	char *PathCStr = CStrFromStr8(Scratch.Arena, Path);

	WIN32_FILE_ATTRIBUTE_DATA Info = {0};
	u64 TimeStamp = 0;
	if (GetFileAttributesExA(PathCStr,
								GetFileExInfoStandard,
								&Info))
	{
		ULARGE_INTEGER FileTime;
		FileTime.HighPart = Info.ftLastWriteTime.dwHighDateTime;
		FileTime.LowPart  = Info.ftLastWriteTime.dwLowDateTime;

		u64 NanoSecondsSince1601 = FileTime.QuadPart * 100ULL;
		TimeStamp = NanoSecondsSince1601 - (EPOCH_DIFF_100NS * 100ULL);
	}

	ScratchEnd(Scratch);
	return TimeStamp;
}

b32
OS_DirectoryTreeIterateWithStack(m_arena *Arena, os_directory_tree Tree, os_directory_tree_iterator *It)
{
	b32 Result = True;

	if(!It->Current)
	{
		It->Current = Tree.Root;
		It->ParentIndex = 0;
		It->Index = 0;

		It->Stack = ArenaPushArrayZero(Arena, s64, Tree.Count);
		It->Stack[It->StackTop++] = It->Index;
	}
	else
	{
		s64 CurrentIndex = It->Index;

		os_directory_entry *Next  = It->Current->Next;
		os_directory_entry *First = It->Current->First;

		if(First)
		{
			It->Stack[It->StackTop++] = CurrentIndex;
			It->ParentIndex = CurrentIndex;
			It->Current = First;
			It->Depth++;
		}
		else if(Next)
		{
			It->ParentIndex = It->Stack[It->StackTop - 1];
			It->Current = Next;
		}
		else
		{
			os_directory_entry *Parent = It->Current->Parent;
			for(; Parent; Parent = Parent->Parent)
			{
				It->Depth--;
				It->ParentIndex = It->Stack[--It->StackTop];
				if(Parent->Next)
				{
					It->ParentIndex = It->Stack[It->StackTop - 1];
					It->Current = Parent->Next;
					break;
				}
			}

			if(!Parent)
			{
				It->Current = Null;
				Result = False;
			}
		}

		if(Result)
		{
			It->Index += 1;
		}
	}

	return Result;
}

b32 OS_DirectoryTreeIterate(os_directory_tree Tree, os_directory_tree_iterator *Iterator)
{
	b32 Result = True;
	if(!Iterator->Current)
	{
		Iterator->Current = Tree.Root;
	}
	else
	{
		os_directory_entry *Next = Iterator->Current->Next;
		os_directory_entry *First = Iterator->Current->First;

		if(First)
		{
			Iterator->Current = First;
			Iterator->Depth++;
		}
		else if(Next)
		{
			Iterator->Current = Next;
		}
		else
		{
			os_directory_entry *Parent = Iterator->Current->Parent;

			for(;Parent; Parent = Parent->Parent)
			{
				Iterator->Depth--;
				if(Parent->Next)
				{
					Iterator->Current = Parent->Next;
					break;
				}
			}
			if(!Parent)
			{
				Iterator->Current = Null;
				Result = False;
			}
		}

		if(Result)
		{
			Iterator->Index += 1;
		}
	}
	return Result;
}

void
OS_WriteStringToDisk(m_arena *Arena, str8 DestinationPath, str8 StringToWrite)
{
	m_scratch_arena Scratch = ScratchBegin(0, 0);
	char *PathCStr = CStrFromStr8(Scratch.Arena, DestinationPath);

	HANDLE FileHandle = CreateFileA(PathCStr,
									GENERIC_WRITE,
									0,
									NULL,
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL);

	if(FileHandle == INVALID_HANDLE_VALUE)
	{
		LogError("Failed to create file");
	}
	else
	{
		DWORD bytesWritten;
		WriteFile(FileHandle,
				  StringToWrite.Bytes,
				  (DWORD)StringToWrite.Size,
				  &bytesWritten,
				  NULL);
		CloseHandle(FileHandle);
	}

	ScratchEnd(Scratch);
}

str8
OS_LoadEntireFileIntoMemory(m_arena *Arena, str8 Filename)
{
	m_scratch_arena Scratch = ScratchBegin(0, 0);
	char *PathCStr = CStrFromStr8(Scratch.Arena, Filename);

	HANDLE FileHandle = CreateFileA(PathCStr,
							   GENERIC_READ,
							   FILE_SHARE_READ,
							   NULL,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   NULL);

	if(FileHandle == INVALID_HANDLE_VALUE)
	{
		LogError("File not found");
		ScratchEnd(Scratch);
		return (str8){0};
	}

	LARGE_INTEGER Size;
	GetFileSizeEx(FileHandle, &Size);

	u8 *Buffer = ArenaPush(Arena, (u64)Size.QuadPart + 1);
	Buffer[Size.QuadPart] = 0;

	DWORD bytesRead;
	ReadFile(FileHandle, Buffer, (DWORD)Size.QuadPart, &bytesRead, NULL);
	CloseHandle(FileHandle);

	str8 result =
	{
		.Bytes = Buffer,
		.Size  = bytesRead
	};

	ScratchEnd(Scratch);
	return result;
}
