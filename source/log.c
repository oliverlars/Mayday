#include "log.h"
#include "memory.h"
#include "strings.h"

logger *LoggerInitialise(m_arena *Arena)
{
	return ArenaPushArrayZero(Arena, logger, 1);
}

void LogInfoF(char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	logger *Logger = ThreadContext->Logger;

	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);


	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("INFO", S_BOLD) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?u64 ", S_YELLOW), LineNumber));

	Str8ListAppend(Logger->Arena, &List, StringToLog);

	str8 Result = Str8ListJoin(Logger->Arena, &List);
	Print("?S\n", Result);
	ArenaReset(Logger->Arena);
}

void LogWarnF(char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	logger *Logger = ThreadContext->Logger;

	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);

	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("WARN", S_BOLD S_YELLOW) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?u64 ", S_YELLOW), LineNumber));

	Str8ListAppend(Logger->Arena, &List, StringToLog);

	str8 Result = Str8ListJoin(Logger->Arena, &List);
	Print("?S\n", Result);
	ArenaReset(Logger->Arena);
}

void LogErrorF(char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	logger *Logger = ThreadContext->Logger;

	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);

	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("Error", S_BOLD S_RED) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?cstr", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("?u64 ", S_YELLOW), LineNumber));

	Str8ListAppend(Logger->Arena, &List, StringToLog);

	str8 Result = Str8ListJoin(Logger->Arena, &List);
	Print("?S\n", Result);
	ArenaReset(Logger->Arena);
}
