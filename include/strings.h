#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "memory.h"

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

#define S_RED     "\x1B[31m"
#define S_GREEN   "\x1B[32m"
#define S_YELLOW  "\x1B[33m"
#define S_BLUE    "\x1B[34m"
#define S_MAGENTA "\x1B[35m"
#define S_CYAN    "\x1B[36m"
#define S_WHITE   "\x1B[37m"
#define S_RESET   "\x1B[0m"
#define S_BOLD    "\033[1m"

#define DeferLoop(Begin, End) for(int _i_ = (Begin, 0); _i_ != 0; (End, _i_++))

#define StringStyle(String, Style) Style String S_RESET

#define Str8(String) (str8){strlen(String), (u8*)String}

b32 CharIsUpper(u8 Char);
b32 CharIsLower(u8 Char);
u8 CharToLower(u8 Char);
u8 CharToUpper(u8 Char);

str8 Str8PushFV(m_arena *Arena, char *Format, va_list InputArgList);
str8 Str8PushF(m_arena *Arena, char *Format, ...);
char* CStrFromStr8(m_arena *Arena, str8 String);
str8 Str8UpToLastCharacter(m_arena *Arena, str8 InputPath, u8 Character);
str8 Str8FromLastCharacter(m_arena *Arena, str8 InputPath, u8 Character);
str8 Str8Copy(m_arena *Arena, str8 String);
b32 Str8Equal(str8 A, str8 B);
b32 Str8CaseInsensitiveEqual(str8 A, str8 B);
b32  Str8EndsWith(str8 String, str8 EndsWith);
b32  Str8StartsWith(str8 String, str8 StartsWith);
b32 Str8HasSubString(str8 Haystack, str8 Needle);
b32 Str8HasCaseInsensitiveSubString(str8 Haystack, str8 Needle);
str8_list Str8Split(m_arena *Arena, str8 String, char SplitBy);
b32 Str8FuzzyMatch(str8 HayStack, str8 Needle);

void Str8ListAppend(m_arena *Arena, str8_list *List, str8 String);
void Str8ListPrepend(m_arena *Arena, str8_list *List, str8 String);
void Str8ListInsertBefore(m_arena *Arena, str8_list *List, str8_list_node* Node, str8 String);
void Str8ListInsertAfter(m_arena *Arena, str8_list *List, str8_list_node* Node, str8 String);
str8 Str8ListJoin(m_arena *Arena, str8_list *List);
str8 Str8ListJoinBy(m_arena *Arena, str8_list *List, char Char);
b32 NextInStr8List(str8_list_node **Node);
int Str8ListCount(str8_list List);

void Print(char *Format, ...);
void PrintLn(char *Format, ...);
