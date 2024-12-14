#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "strings.h"
#include "memory.h"

#include "maths.h"



str8 Str8PushFV(memory_arena *Arena, char *Format, va_list InputArgList)
{
	va_list ArgList;
	va_copy(ArgList, InputArgList);
	str8_list Builder = {0};
	char *Iter = Format;
	char *Start = Format;

	while(Iter && Iter[0])
	{
		if(Iter[0] && *Iter++ == '?')
		{
			str8 UpTo = {Iter - Start - 1, Start};
			Str8ListAppend(Arena, &Builder, UpTo);

			if(0) {}
			else if(Str8Equal((str8){3, Iter}, Str8("v3f")))
			{
				v3f V = va_arg(ArgList, v3f);
				int BytesNeeded = snprintf(0, 0, "[%f, %f, %f]", V.X, V.Y, V.Z) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "[%f, %f, %f]", V.X, V.Y, V.Z);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){1, Iter}, Str8("S")))
			{
				str8 S = va_arg(ArgList, str8);
				int BytesNeeded = snprintf(0, 0, "%.*s", (int)S.Size, S.Bytes) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%.*s", (int)S.Size, S.Bytes);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 1;
				Start = Iter;
			}
			else if(Str8Equal((str8){4, Iter}, Str8("cstr")))
			{
				char *S = va_arg(ArgList, char*);
				int BytesNeeded = snprintf(0, 0, "%s", S) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%s", S);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 4;
				Start = Iter;
			}
			else if(Str8Equal((str8){2, Iter}, Str8("s8")))
			{
				int S8 = va_arg(ArgList, int);
				int BytesNeeded = snprintf(0, 0, "%hhd", S8) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%hhd", S8);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 2;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("s16")))
			{
				int S16 = va_arg(ArgList, int);
				int BytesNeeded = snprintf(0, 0, "%hd", S16) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%hd", S16);
				Str8ListAppend(Arena, &Builder, Value);
				
				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("s32")))
			{
				int S32 = va_arg(ArgList, int);
				int BytesNeeded = snprintf(0, 0, "%d", S32) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%d", S32);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("s64")))
			{
				s64 S64 = va_arg(ArgList, s64);
				int BytesNeeded = snprintf(0, 0, "%lld", S64) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%lld", S64);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){2, Iter}, Str8("u8")))
			{
				unsigned int U8 = va_arg(ArgList, unsigned int);
				int BytesNeeded = snprintf(0, 0, "%hhu", U8) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%hhu", U8);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 2;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("u16")))
			{
				unsigned int U16 = va_arg(ArgList, unsigned int);
				int BytesNeeded = snprintf(0, 0, "%hu", U16) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%hu", U16);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("u32")))
			{
				unsigned int U32 = va_arg(ArgList, unsigned int);
				int BytesNeeded = snprintf(0, 0, "%u", U32) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%u", U32);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("u64")))
			{
				unsigned long long U64 = va_arg(ArgList, unsigned int);
				int BytesNeeded = snprintf(0, 0, "%llu", U64) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%llu", U64);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("f32")))
			{
				float F32 = va_arg(ArgList, double);
				int BytesNeeded = snprintf(0, 0, "%f", F32) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%f", F32);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
			else if(Str8Equal((str8){3, Iter}, Str8("f64")))
			{
				float F64 = va_arg(ArgList, double);
				int BytesNeeded = snprintf(0, 0, "%f", F64) + 1;
				str8 Value = {0};
				Value.Bytes = ArenaPush(Arena, BytesNeeded);
				Value.Size = snprintf((char*)Value.Bytes, BytesNeeded, "%f", F64);
				Str8ListAppend(Arena, &Builder, Value);

				Iter += 3;
				Start = Iter;
			}
		}
	}
	va_end(ArgList);
	
	
	str8 UpTo = {Iter - Start, Start};
	Str8ListAppend(Arena, &Builder, UpTo);

	str8 Result = Str8ListJoin(Arena, &Builder);
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

str8 Str8Copy(memory_arena *Arena, str8 String)
{
	str8 Result = {0};
	Result.Size = String.Size;
	Result.Bytes = ArenaPush(Arena, String.Size);
	MemoryCopy(Result.Bytes, String.Bytes, String.Size);
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
	if(String.Size <= 0 ) return;
	str8_list_node *Node = ArenaPushTypeZero(Arena, str8_list_node);
	Node->String = Str8Copy(Arena, String);
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
	Node->String = Str8Copy(Arena, String);
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
		Insert->String = Str8Copy(Arena, String);
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
		Insert->String = Str8Copy(Arena, String);
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
	u8 *Bytes = ArenaPush(Arena, BytesToAllocate);
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


void Print(char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	u64 At = G_ScratchArena.At;
	str8 Result = Str8PushFV(&G_ScratchArena, Format, Args);
	va_end(Args);
	printf("%.*s", (int)Result.Size, Result.Bytes);
	// for (u64 I = 0; I < Result.Size; I++)
	// {
	// 	printf("%02X ", (unsigned char)Result.Bytes[I]);
	// }
	G_ScratchArena.At = At;
}

void PrintLn(char *Format, ...)
{
	va_list args;
	va_start(args, Format);
	u64 At = G_ScratchArena.At;
	str8 Result = Str8PushFV(&G_ScratchArena, Format, args);
	va_end(args);
	printf("%.*s\n", (int)Result.Size, Result.Bytes);
	G_ScratchArena.At = At;
}