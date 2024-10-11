#include "include/first_win32.h"




int main() {

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    u8* SomeMemory = ArenaPush(&Arena, 6);
    str8 String = Str8PushF(&Arena, "Hello World");
    printf("%.*s", Str8ToPrintfArgs(String));


    str8_list List = {0};

    Str8ListAppend(&Arena, &List, Str8Lit("Hello"));
    Str8ListAppend(&Arena, &List, Str8Lit(", "));
    Str8ListAppend(&Arena, &List, Str8Lit("World"));
    printf("\n");

    for(str8_list_node *Node = List.First; Node != 0; Node = Node->Next) {
        printf("%.*s", Str8ToPrintfArgs(Node->String));
    }

    printf("\n");
    Str8ListPopN(&List, 1);

    for(str8_list_node *Node = List.First; Node != 0; Node = Node->Next) {
        printf("%.*s", Str8ToPrintfArgs(Node->String));
    }

    printf("\n");
    Str8ListPopN(&List, 1);

    for(str8_list_node *Node = List.First; Node != 0; Node = Node->Next) {
        printf("%.*s", Str8ToPrintfArgs(Node->String));
    }

    printf("\n");
    Str8ListPopN(&List, 0);

    for(str8_list_node *Node = List.First; Node != 0; Node = Node->Next) {
        printf("%.*s", Str8ToPrintfArgs(Node->String));
    }

    return 0;
}