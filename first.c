#include "include/first.h"




int main() {

    memory_arena Arena = {0};
    Arena.Start = MemoryReserve(GigaBytes(64));

    u8* SomeMemory = ArenaPush(&Arena, 6);
    SomeMemory[0] = 'H';
    SomeMemory[1] = 'E';
    SomeMemory[2] = 'L';
    SomeMemory[3] = 'L';
    SomeMemory[4] = 'O';
    SomeMemory[5] = 0;
    printf(SomeMemory);

    return 0;
}