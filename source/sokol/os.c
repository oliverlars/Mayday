#include "log.h"
#include "memory.h"
#include <assert.h>
#include <errno.h>
#define SOKOL_NO_ENTRY
#define SOKOL_METAL
#include "sokol_app.h"

#include <mach/mach_time.h>

#include "strings.h"
#include "os.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>



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
		.window_title = (char*)Window.Title.Bytes,
		.init_userdata_cb = OS_Initialise,
		.frame_userdata_cb = OS_Update,
		.event_userdata_cb = OS_Event,
		.width = Window.Width,
		.height = Window.Height,
		.user_data = Data,
		.high_dpi = true,
	};
	sapp_run(&SAPPDesc);
}

os_handle OS_GetWindowHandle()
{
	return (os_handle)sapp_macos_get_window();
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
	return mach_absolute_time();
}

f32 OS_MilliSecondsFromTimeStamp(u64 TimeStamp)
{
	mach_timebase_info_data_t Info;
	mach_timebase_info(&Info);

	return (f32)((TimeStamp) * Info.numer / Info.denom) / 1000000.0;
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
	struct stat StatBuf;
	b32 Result = False;
	if (stat(CStrFromStr8(Scratch.Arena, Path), &StatBuf) == 0) {
		Result = S_ISDIR(StatBuf.st_mode);
	}
	ScratchEnd(Scratch);
	return Result;
}

s64 OS_DirectoryTreeFromPathHelper(m_arena *Arena, str8 Path, DIR *Directory, os_directory_entry *Entries)
{
	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);
	struct dirent *Entry;
	s64 Count = 0;
	while((Entry = readdir(Directory)))
	{
		if(Entry->d_name[0] != '.')
		{
			os_directory_entry DirectoryEntry = {0};
			str8 FullPath = Str8PushF(Arena, "?S/?cstr", Path, Entry->d_name);
			if(OS_IsPathDirectory(FullPath))
			{
				DirectoryEntry.PathAndName = FullPath;
				DirectoryEntry.Type = OS_DIR_ENTRY_FOLDER;
				InsertDirectoryEntryAsChild(Arena, Entries, DirectoryEntry);
				DIR *NextDirectory = opendir(CStrFromStr8(Scratch.Arena, FullPath));
				Count += OS_DirectoryTreeFromPathHelper(Arena, FullPath, NextDirectory, Entries->Last);
				closedir(NextDirectory);
			}
			else
			{
				DirectoryEntry.Type = OS_DIR_ENTRY_FILE;
				DirectoryEntry.PathAndName = FullPath;
				InsertDirectoryEntryAsChild(Arena, Entries, DirectoryEntry);
			}
			Count += 1;
		}
	}
	ScratchEnd(Scratch);
	return Count;
}

os_directory_tree OS_DirectoryTreeFromPath(m_arena *Arena, str8 Path)
{
	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);


	os_directory_entry *EntryRoot = ArenaPushArrayZero(Arena, os_directory_entry, 1);
	EntryRoot->PathAndName = Str8Copy(Arena, Path);
	EntryRoot->Type = OS_DIR_ENTRY_FOLDER;
	DIR *Directory = opendir(CStrFromStr8(Scratch.Arena, Path));
	os_directory_tree Result = {0};
	if(!Directory)
	{
		LogError("?s32", errno);
	}
	else
	{
		s64 Count = OS_DirectoryTreeFromPathHelper(Arena, Path, Directory, EntryRoot);
		closedir(Directory);
		Result.Count = Count + 1;
		Result.Root = EntryRoot;
	}

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

str8 OS_LoadEntireFileIntoMemory(m_arena *Arena, str8 Filename)
{
	s32 FileHandle = open(CStrFromStr8(Arena, Filename), O_RDONLY);
	if (FileHandle == -1)
	{
		LogError("File not found");
		return (str8){0};
	}

	struct stat FileStat;
	fstat(FileHandle, &FileStat);

	u8 *BackingMemory = ArenaPush(Arena, (u64)FileStat.st_size + 1);
	BackingMemory[FileStat.st_size] = 0;

	str8 Result = {0};
	Result.Bytes = BackingMemory;
	Result.Size = FileStat.st_size;

	ssize_t BytesRead = read(FileHandle, BackingMemory, FileStat.st_size);

	return Result;
}
