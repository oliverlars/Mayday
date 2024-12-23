#define SOKOL_NO_ENTRY
#define SOKOL_METAL
#include "sokol_app.h"

#include <mach/mach_time.h>

#include "strings.h"
#include "os.h"



os_window OS_MakeWindow(str8 Name, int Width, int Height)
{
	os_window Window = {0};
	Window.Width = Width;
	Window.Height = Height;
	Window.Title = Name;

	return Window;
}

void OS_Run(os_window Window, void *Data)
{
	sapp_desc SAPPDesc = (sapp_desc)
	{
		.window_title = (char*)Window.Title.Bytes,
		.init_userdata_cb = OS_Initialise,
		.frame_userdata_cb = OS_Update,
		.event_userdata_cb = OS_Event,
		.width = Window.Width,
		.height = Window.Height,
		.user_data = Data,
		.high_dpi = true,
	};
	sapp_run(&SAPPDesc);
}

os_handle OS_GetWindowHandle()
{
	return (os_handle)sapp_macos_get_window();
}

int OS_WindowWidth(os_window *Window)
{
	return sapp_width();
}
int OS_WindowHeight(os_window *Window)
{
	return sapp_height();
}


u64 OS_TimeStampNow()
{
	return mach_absolute_time();
}

f32 OS_MilliSecondsFromTimeStamp(u64 TimeStamp)
{
	mach_timebase_info_data_t Info;
	mach_timebase_info(&Info);

	return (f32)((TimeStamp) * Info.numer / Info.denom) / 1000000.0;
}