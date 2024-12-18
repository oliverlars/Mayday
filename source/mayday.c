
#include "draw.h"
#include "log.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "draw.h"
#include "berkeley_mono_font.h"
#include "render.h"

typedef struct mayday
{
	memory_arena *FrameArena;
	memory_arena *PermanentArena;
	os_window Window;
	r_context R_Context;
	d_context D_Context;

	d_font Font;
}
mayday;

#include "shaders.h"

void OS_Update(void *Data)
{
	mayday *Application = Data;

	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = OS_WindowHeight(&Application->Window);

	D_DrawStr8(&Application->R_Context, &Application->D_Context,
			   Application->FrameArena, Application->Font,
			   v2f(50,  (f32)WindowHeight - 100), v4f(1, 0.5, 0, 1),
			   50, (str8){sizeof(G_TriangleShader), G_TriangleShader});

	D_GlyphsToAtlas(&Application->D_Context);

	R_TextureFromBytes(&Application->R_Context, Application->D_Context.FontAtlas, D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);

	R_Update(&Application->R_Context);

	Application->FrameArena->At = 0;

	ArenaReset(Application->FrameArena);
}

void OS_Initialise(void *Data)
{
	mayday *Application = Data;
	R_Init(&Application->R_Context);
}

int main()
{
	memory_arena Arena = {0};
	memory_arena FrameArena = {0};

	logger Logger = {0};
	Logger.Silent = True;
	Logger.Arena = &Arena;

	mayday Application = {0};
	
	Application.PermanentArena = &Arena;
	Application.FrameArena = &FrameArena;

	Application.R_Context.Arena = &Arena;
	Application.R_Context.Logger = &Logger;

	Application.D_Context.Arena = &Arena;
	Application.D_Context.Logger = &Logger;

	Application.Window = OS_MakeWindow(Str8("Mayday"), 3024, 1964);

	Application.Font = D_LoadFontFromTTF(&Application.D_Context, G_BerkeleyMonoRegular);

	OS_Run(Application.Window, &Application);
}