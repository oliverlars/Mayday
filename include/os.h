#include "strings.h"

#include "sokol_app.h"

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
void OS_Run(os_window Window, void *Data);

void OS_Update(void *Data);
void OS_Initialise(void *Data);
void OS_Event(const sapp_event *Event, void *Data);



os_handle OS_GetWindowHandle();
int OS_WindowWidth(os_window *Window);
int OS_WindowHeight(os_window *Window);
