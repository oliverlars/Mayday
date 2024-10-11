#include "include/first_win32.h"




int main() {

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    u8* SomeMemory = ArenaPush(&Arena, 6);
    str8 String = Str8PushF(&Arena, "Hello World");
    printf("%.*s", Str8ToPrintfArgs(String));


    str8_list List = {0};

    Str8ListAppend(&Arena, &List, Str8Lit("Hello"));
    Str8ListAppend(&Arena, &List, Str8Lit("World"));
    printf("\n");
    printf("%.*s\n", Str8ToPrintfArgs(List.Last->Previous->String));
    printf("%.*s\n", Str8ToPrintfArgs(List.First->Next->String));

    return 0;
}