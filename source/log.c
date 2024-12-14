#include "log.h"
#include "strings.h"

#include <stdio.h>

void LogInfoF(logger *Logger, char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);


	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("INFO", S_BOLD) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%cstr", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%cstr", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%u64", S_YELLOW), LineNumber));

	Str8ListAppend(Logger->Arena, &List, StringToLog);

	str8 Result = Str8ListJoin(Logger->Arena, &List);
	printf("%.*s\n\n", (int)Result.Size, Result.Bytes);
}

void LogWarnF(logger *Logger, char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);

	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("WARN", S_BOLD S_YELLOW) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%s", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%s", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%llu", S_YELLOW), LineNumber));
	
	Str8ListAppend(Logger->Arena, &List, StringToLog);
	
	str8 Result = Str8ListJoin(Logger->Arena, &List);
	printf("%.*s\n\n", (int)Result.Size, Result.Bytes);
}

void LogErrorF(logger *Logger, char *FileName, char *FunctionName, u64 LineNumber, char *Format, ...)
{
	if(Logger->Silent) return;

	va_list args;
	va_start(args, Format);
	str8 StringToLog = Str8PushFV(Logger->Arena, Format, args);
	va_end(args);

	str8_list List = {0};
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, "[" StringStyle("Error", S_BOLD S_RED) "] "));

	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%s", S_BLUE) "() at ", FunctionName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%s", S_GREEN) ":", FileName));
	Str8ListAppend(Logger->Arena, &List, Str8PushF(Logger->Arena, StringStyle("%llu", S_YELLOW), LineNumber));

	Str8ListAppend(Logger->Arena, &List, StringToLog);

	str8 Result = Str8ListJoin(Logger->Arena, &List);
	printf("%.*s\n\n", (int)Result.Size, Result.Bytes);
}