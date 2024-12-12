#pragma once

#include "strings.h"
#include "memory.h"


typedef struct logger_info
{
    str8 FunctionName;
    u64 LineNumber;
    str8 FileName;

    struct logger_info *Previous;
}
logger_info;

typedef struct logger
{
    b32 Silent;
    memory_arena *Arena;
    str8_list *Messages;
    int Level;

    logger_info *Info;
}
logger;


void LogInfoF(logger *Logger, char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);
void LogWarnF(logger *Logger, char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);
void LogErrorF(logger *Logger, char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);

void LogPushFunction(logger *Logger, char* FileName, u64 LineNumber, char* FunctionName);


#define LogInfo(Logger, ...) LogInfoF(Logger, (char*)__FILE__, (char*)__func__, (u64)__LINE__, __VA_ARGS__)
#define LogWarn(Logger, ...) LogWarnF(Logger, (char*)__FILE__, (char*)__func__, (u64)__LINE__, __VA_ARGS__)
#define LogError(Logger, ...) LogErrorF(Logger, (char*)__FILE__, (char*)__func__, (u64)__LINE__, __VA_ARGS__)