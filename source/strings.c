#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "strings.h"
#include "memory.h"
#include "custom_stb_sprintf.h"

str8 Str8PushFV(memory_arena *Arena, char *Format, va_list InputArgList)
{
	va_list ArgList;
	va_copy(ArgList, InputArgList);
	int BytesNeeded = stbsp_sprintf(0, 0, Format, InputArgList) + 1;
	str8 Result = {0};
	Result.Bytes = ArenaPush(Arena, BytesNeeded);
	Result.Size = stbsp_snprintf((char*)Result.Bytes, BytesNeeded, Format, ArgList);
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
	if(String.Bytes[String.Size] == '\0')
	{
		NullTerminated = 1;
	}
   
	char* Result = (char*)ArenaPushZero(Arena, String.Size + NullTerminated);
	MemoryCopy(Result, String.Bytes, String.Size);
	return Result;
}

b32 Str8Equal(str8 A, str8 B)
{
	if(A.Size != B.Size) return False;
	for(u64 Index = 0; Index < A.Size; Index++)
	{
		if(A.Bytes[Index] != B.Bytes[Index])
		{
			return False;
		}
	}
	return True;
}

void Str8ListAppend(memory_arena *Arena, str8_list *List, str8 String)
{
	str8_list_node *Node = ArenaPushTypeZero(Arena, str8_list_node);
	Node->String = Str8PushF(Arena, "%S", String);
	if(List->First == Null)
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
	Node->String = Str8PushF(Arena, "%S", String);
	if(List->First == Null)
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
	if(Node->Previous != Null)
	{
		str8_list_node *Insert = ArenaPushTypeZero(Arena, str8_list_node);
		Insert->String = Str8PushF(Arena, "%S", String);
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
	if(Node->Next != Null)
	{
		str8_list_node *Insert = ArenaPushTypeZero(Arena, str8_list_node);
		Insert->String = Str8PushF(Arena, "%S", String);
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

str8 Str8ListJoin(memory_arena *Arena, str8_list *List)
{
	u64 BytesToAllocate = 0;
	for(str8_list_node *Node = List->First; Node != Null; Node = Node->Next)
	{
		BytesToAllocate += Node->String.Size;
	}
	u8* Bytes = ArenaPush(Arena, BytesToAllocate);
	u64 At = 0;
	for(str8_list_node *Node = List->First; Node != Null; Node = Node->Next)
	{
		MemoryCopy(Bytes + At, Node->String.Bytes, Node->String.Size);
		At += Node->String.Size;
	}
	str8 Result = { .Bytes = Bytes, .Size = BytesToAllocate };

	return Result;
}

b32 NextInStr8List(str8_list_node **Node)
{
	if(*Node != Null)
	{
		*Node = (*Node)->Next;
		return True;
	}
	return False;
}