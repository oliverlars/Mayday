#include "strings.h"

typedef void* os_handle;

typedef struct os_window
{
	int Width;
	int Height;
	

	os_handle Handle;
	str8 Title;
}
os_window;

os_window OS_MakeWindow(str8 Name, int Width, int Height);
void OS_Run(os_window Window);

void OS_Update();

void OS_Initialise();
