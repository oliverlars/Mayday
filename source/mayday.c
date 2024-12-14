
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "maths.h"
#include "render.h"

typedef struct mayday
{
	os_window Window;
	r_context RenderContext;
}
mayday;


void OS_Update(void *Data)
{
	mayday *Application = Data;

	R_Update(&Application->RenderContext);
}

void OS_Initialise(void *Data)
{
	mayday *Application = Data;
	R_Init(&Application->RenderContext);
}

#include <stdio.h>

int main()
{
	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));
	//mayday Application = {0};
	//Application.RenderContext.Logger.Arena = &Arena;
	//Application.Window = OS_MakeWindow(Str8("Mayday"), 1280, 720);

	str8 Test = Str8PushF(&Arena, "?v3f \n", (v3f){1, 2, 3});
	PrintLn("?S", Test);

	//OS_Run(Application.Window, &Application);
}