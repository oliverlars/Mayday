#pragma once
#include "strings.h"

#include "sokol_app.h"

typedef void* os_handle;

typedef struct os_window
{
	int Width;
	int Height;

	os_handle Handle;
	str8 Title;
}
os_window;


#if defined(__APPLE__)
#define OS_DARWIN OS_DARWIN
#elif defined(_WIN32)
#define OS_WINDOWS OS_WINDOWS
#endif 

typedef enum os_directory_entry_type
{
	OS_DIR_ENTRY_UNKNOWN,
	OS_DIR_ENTRY_FILE,
	OS_DIR_ENTRY_FOLDER,
} os_directory_entry_type;

typedef struct os_directory_entry
{
	str8 PathAndName;
	os_directory_entry_type Type;
	struct os_directory_entry *First;
	struct os_directory_entry *Last;

	struct os_directory_entry *Next;
	struct os_directory_entry *Parent;
}
os_directory_entry;

typedef struct os_directory_tree
{
	os_directory_entry *Root;
	s64 Count;
} os_directory_tree;

typedef struct os_directory_tree_iterator
{
	s64 *Stack;
	s64 StackTop;

	s64 ParentIndex;
	s64 Index;
	s64 Depth;
	os_directory_entry *Current;
} os_directory_tree_iterator;

os_window OS_MakeWindow(str8 Name, int Width, int Height);
void OS_Run(os_window Window, void *Data);

u64 OS_TimeStampNow();

f32 OS_MilliSecondsFromTimeStamp(u64 TimeStamp);

void OS_Update(void *Data);
void OS_Initialise(void *Data);
void OS_Event(const sapp_event *Event, void *Data);
void OS_Exit(void *Data);


str8 OS_GetLastModifiedTime(m_arena *Arena, str8 Path);
u64 OS_GetFileLastWriteTimestamp(str8 Path);

os_handle OS_GetWindowHandle();
int OS_WindowWidth(os_window *Window);
int OS_WindowHeight(os_window *Window);


typedef enum operating_system
{
	OS_WINDOWS,
	OS_DARWIN,
	OS_LINUX,
}
operating_system;

operating_system OS_GetOperatingSystem();


typedef struct os_directory_tree_options
{
	b32 FoldersOnly;
	int MaxDepth; // 0 = unlimited
} os_directory_tree_options;


str8 OS_LoadEntireFileIntoMemory(m_arena *Arena, str8 Filename);
void OS_WriteStringToDisk(m_arena *Arena, str8 DestinationPath, str8 StringToWrite);


typedef struct os_directory_stats
{
	int FileCount;
	int FolderCount;
	u64 TotalSizeBytes;
} os_directory_stats;

os_directory_stats OS_DirectoryStats(str8 Path);

b32 OS_ValidPath(str8 Path);
b32 OS_DirectoryTreeIterate(os_directory_tree Tree, os_directory_tree_iterator *Iterator);
b32 OS_DirectoryTreeIterateWithStack(m_arena *Arena, os_directory_tree Tree, os_directory_tree_iterator *Iterator);
int OS_CountAllEntriesInDirectoryTree(os_directory_tree Tree);
os_directory_tree OS_DirectoryTreeFromPath(m_arena *Arena, str8 Path, os_directory_tree_options Options);
