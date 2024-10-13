#include "first_win32.h"

int main()
{

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    str8_list SourceFiles = {0};
    Str8ListAppend(&Arena, &SourceFiles, Str8("src/mayday.c"));
    Str8ListAppend(&Arena, &SourceFiles, Str8("src/abc.c"));
    

    str8_list IncludePaths = {0};
    Str8ListAppend(&Arena, &IncludePaths, Str8("include/"));
    build_options Options = {
        .Compiler = COMPILER_MSVC,
        .OutputName = Str8("mayday.exe"),
        .OutputType = OUTPUT_EXECUTABLE,
        .SourceFiles = SourceFiles,
        .IncludePaths = IncludePaths,
        .WorkingDirectory = GetExecutableDirectory(&Arena),
        .OutputPath = Str8("build/")
    };

    CompileFromBuildOptions(&Arena, Options);

    return 0;
}