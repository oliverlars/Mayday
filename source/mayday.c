
#include "os.h"
#include "strings.h"


void OS_Update()
{

}

void OS_Initialise()
{
	
}

int main()
{
	os_window Window = OS_MakeWindow(Str8("Mayday"), 1280, 720);

	OS_Run(Window);
}