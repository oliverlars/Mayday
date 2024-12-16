
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
	os_window Window;
	r_context R_Context;
	d_context D_Context;

	d_font Font;
}
mayday;


void OS_Update(void *Data)
{
	mayday *Application = Data;

	d_atlas_glyph *Glyph = D_GlyphFromCodePoint(&Application->D_Context, Application->Font, 250, 'G');
	Glyph = D_GlyphFromCodePoint(&Application->D_Context, Application->Font, 250, 'G');
	Glyph = D_GlyphFromCodePoint(&Application->D_Context, Application->Font, 250, 'H');


	D_GlyphsToAtlas(&Application->D_Context);
	D_GlyphsToAtlas(&Application->D_Context);

	R_TextureFromBytes(&Application->R_Context, Application->D_Context.FontAtlas, D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);

	R_Update(&Application->R_Context);
}

void OS_Initialise(void *Data)
{
	mayday *Application = Data;
	R_Init(&Application->R_Context);
}

int main()
{
	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));

	logger Logger = {0};
	Logger.Arena = &Arena;

	mayday Application = {0};
	
	Application.R_Context.Arena = &Arena;
	Application.R_Context.Logger = &Logger;

	Application.D_Context.Arena = &Arena;
	Application.D_Context.Logger = &Logger;

	Application.Window = OS_MakeWindow(Str8("Mayday"), 1280, 720);

	Application.Font = D_LoadFontFromTTF(&Application.D_Context, G_BerkeleyMonoRegular);

	OS_Run(Application.Window, &Application);
}