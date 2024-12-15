
#include "draw.h"
#include "log.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "draw.h"
#include "berkeley_mono_font.h"
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

int main()
{
	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));

	d_context D_Context = {0};
	D_Context.Arena = &Arena;
	D_Context.Logger.Arena = &Arena;

	d_font Font = D_LoadFontFromTTF(&D_Context, G_BerkeleyMonoRegular);
	d_atlas_glyph *Glyph = D_GlyphFromCodePoint(&D_Context, Font, 10, 'G');

	mayday Application = {0};
	Application.RenderContext.Logger.Arena = &Arena;
	Application.Window = OS_MakeWindow(Str8("Mayday"), 1280, 720);

	OS_Run(Application.Window, &Application);
}