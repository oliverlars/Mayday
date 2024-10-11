#include "first_win32.h"

int main()
{

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    str8_list SourceFiles = {0};
    Str8ListAppend(&Arena, &SourceFiles, Str8("src/mayday.c"));

    str8_list IncludePaths = {0};
    Str8ListAppend(&Arena, &IncludePaths, Str8("include/"));
    build_options Options = {
        .Compiler = COMPILER_MSVC,
        .OutputName = Str8("Mayday"),
        .OutputType = OUTPUT_EXECUTABLE,
        .SourceFiles = SourceFiles,
        .IncludePaths = IncludePaths,
    };


    str8 TestFile = Str8("#include \"ABC.h\" \n\n #include \"DEF.h\"");
    str8 TestFile2 = LoadEntireFileIntoMemory(&Arena, Str8("first.c"));
    printf("%.*s\n\n", Str8ToPrintfArgs(TestFile2));
    str8_list ListOfIncludes = {0};
    FindAllIncludesInFile(&Arena, TestFile2, &ListOfIncludes);

    printf("\n\nList of includes: %.*s\n\n", Str8ToPrintfArgs(Str8ListJoin(&Arena, &ListOfIncludes)));

    CompileFromBuildOptions(&Arena, Options);

    return 0;
}