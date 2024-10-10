#include "include/first.h"




int main() {

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    u8* SomeMemory = ArenaPush(&Arena, 6);
    str8 String = Str8PushF(&Arena, "Hello World");
    printf("%.*s", Str8ToPrintfArgs(String));

    return 0;
}