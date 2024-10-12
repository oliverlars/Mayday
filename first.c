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
        .OutputName = Str8("Mayday"),
        .OutputType = OUTPUT_EXECUTABLE,
        .SourceFiles = SourceFiles,
        .IncludePaths = IncludePaths,
        .WorkingDirectory = GetExecutableDirectory(&Arena),
    };


    str8 TestFile = Str8("#include \"ABC.h\" \n\n #include \"DEF.h\"");
    str8 TestFile2 = LoadEntireFileIntoMemory(&Arena, Str8("first.c"));

    str8_list IncludeChainFiles = {0};
    Str8ListAppend(&Arena, &IncludeChainFiles, TestFile2);
    str8_list IncludeChainFilenames = {0};
    Str8ListAppend(&Arena, &IncludeChainFilenames, Str8("src/mayday.c"));
    Str8ListAppend(&Arena, &IncludeChainFilenames, Str8("src/abc.c"));
    FindAllIncludesInFile(&Arena, TestFile2, Options.IncludePaths, &IncludeChainFilenames, &IncludeChainFiles);

    u64 Hashes[4096] = {0};
    u64 Hash = FNV_HASH_START;
    for(str8_list_node *Node = IncludeChainFiles.First; Node != 0; Node = Node->Next)
    {
        FNVHash64(&Hash, Node->String);
    }
    Hashes[0] = Hash;
    DumpHashedDataToFile((u8*)Hashes, 4);


    printf("\nList of includes: %.*s\n\n", Str8ToPrintfArgs(Str8ListJoin(&Arena, &IncludeChainFilenames)));

    CompileFromBuildOptions(&Arena, Options);

    return 0;
}