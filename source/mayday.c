
#include "draw.h"
#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "draw.h"
#include "berkeley_mono_font.h"
#include "render.h"

#include "edit.h"

typedef struct mayday
{
	memory_arena *FrameArena;
	memory_arena *PermanentArena;
	os_window Window;
	r_context R_Context;
	d_context D_Context;
	e_context E_Context;

	d_font Font;

	editable_buffer Buffer;
	cursor Cursor;
}
mayday;

#include "shaders.h"

void OS_Update(void *Data)
{
	mayday *Application = Data;

	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = OS_WindowHeight(&Application->Window);

	v2f BufferPosition = v2f(50, (f32)WindowHeight - 100);

	Application->Buffer.HeightInPixels = 50;

	Application->Buffer.String = (str8){sizeof(G_TriangleShader), G_TriangleShader};

	Application->Cursor.TargetPosition = E_GetPositionFromEditableBuffer(&Application->D_Context,
																	   &Application->Cursor,
																	   Application->Buffer,
																	   Application->Font,
																	   BufferPosition);
	Application->Cursor.Position = LerpV2f(Application->Cursor.Position, Application->Cursor.TargetPosition, 0.3);

	f32 LineHeight = D_LineHeightFromFont(Application->Font, Application->Buffer.HeightInPixels);

	D_DrawQuad(&Application->D_Context, Application->FrameArena,
	Application->Cursor.Position, 5, LineHeight, v4f(0, 1, 0, 1));

	D_DrawStr8(&Application->D_Context,
			   Application->FrameArena, Application->Font,
			   BufferPosition, v4f(1, 0.5, 0, 1),
			   50, Application->Buffer.String);

	D_GlyphsToAtlas(&Application->D_Context);
	R_TextureFromBytes(&Application->R_Context, Application->D_Context.FontAtlas, D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);

	R_Update(&Application->R_Context);

	Application->FrameArena->At = 0;

	ArenaReset(Application->FrameArena);
}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday *Application = (mayday*)Data;
	if(Event->type == SAPP_EVENTTYPE_KEY_DOWN)
	{	

		if(Event->key_code == SAPP_KEYCODE_BACKSPACE)
		{
			E_RemoveTextFromTextRange(&Application->E_Context, Application->Cursor, &Application->Buffer,
							   Application->Cursor.At-1, Application->Cursor.At);
			Application->Cursor.At -= 1;
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			Application->Cursor.At += 1;
		}

		if(Event->key_code == SAPP_KEYCODE_LEFT)
		{
			Application->Cursor.At -= 1;
		}

		if(Event->key_code == SAPP_KEYCODE_DOWN)
		{
			int DistanceFromNewLine = E_DistanceFromNextNewLine(&Application->D_Context, &Application->Cursor, Application->Buffer);
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&Application->D_Context, &Application->Cursor, Application->Buffer);

			Application->Cursor.At += DistanceFromNewLine;
			Application->Cursor.At += DistanceFromPreviousNewLine;
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&Application->D_Context, &Application->Cursor, Application->Buffer);
			int DistanceFromNewLine = E_DistanceFromNextNewLine(&Application->D_Context, &Application->Cursor, Application->Buffer);
			Application->Cursor.At -= DistanceFromPreviousNewLine;
			Application->Cursor.At -= 1;
			Application->Cursor.At -= E_DistanceFromPreviousNewLine(&Application->D_Context, &Application->Cursor, Application->Buffer);
			Application->Cursor.At += DistanceFromPreviousNewLine;
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_CHAR)
	{
		if(Event->char_code == 127)
		{
			// HACK(Oliver): Why the hell do i need to do this?
		}
		else if(Event->char_code == 13)
		{
			E_InsertTextAtCursor(&Application->E_Context, Application->Cursor, &Application->Buffer, Str8("\n"));
			Application->Cursor.At += 1;
		}
		else
		{
			E_InsertTextAtCursor(&Application->E_Context, Application->Cursor, &Application->Buffer, Str8PushF(Application->FrameArena, "?char", Event->char_code));
			Application->Cursor.At += 1;
		}
	}
}

void OS_Initialise(void *Data)
{
	mayday *Application = Data;
	R_Init(&Application->R_Context);
	//Application->R_Context.Logger->Silent = True;
}

int main()
{
	memory_arena Arena = {0};
	memory_arena FrameArena = {0};

	logger Logger = {0};
	//Logger.Silent = True;
	Logger.Arena = &Arena;

	mayday Application = {0};
	
	Application.PermanentArena = &Arena;
	Application.FrameArena = &FrameArena;

	Application.R_Context.Arena = &Arena;
	Application.R_Context.Logger = &Logger;


	Application.D_Context.Arena = &Arena;
	Application.D_Context.Logger = &Logger;
	Application.D_Context.R_Context = &Application.R_Context;

	Application.Window = OS_MakeWindow(Str8("Mayday"), 3024, 1964);

	Application.Font = D_LoadFontFromTTF(&Application.D_Context, G_BerkeleyMonoRegular);

	OS_Run(Application.Window, &Application);
}