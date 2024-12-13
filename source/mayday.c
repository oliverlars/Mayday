
#include "os.h"
#include "strings.h"

#include "render.h"

typedef struct mayday
{
	os_window Window;
}
mayday;


void OS_Update(void *Data)
{
	mayday *Application = Data;

	R_Update();
}

void OS_Initialise(void *Data)
{
	mayday *Application = Data;
	R_Init();
}

int main()
{
	mayday Application = {0};

	Application.Window = OS_MakeWindow(Str8("Mayday"), 1280, 720);


	OS_Run(Application.Window, &Application);
}