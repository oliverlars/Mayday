#define SOKOL_NO_ENTRY
#define SOKOL_METAL
#include "sokol_app.h"

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
        .window_title = Window.Title.Bytes,
        .init_userdata_cb = OS_Initialise,
        .frame_userdata_cb = OS_Update,
        .width = Window.Width,
        .height = Window.Height,
        .user_data = Data,
    };
    sapp_run(&SAPPDesc);
}

os_handle OS_GetWindowHandle()
{
    return (os_handle)sapp_macos_get_window();
}


