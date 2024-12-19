
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

typedef struct mayday_context
{
	memory_arena *FrameArena;
	memory_arena *PermanentArena;
	os_window Window;
	render_context Render;
	draw_context Draw;
	edit_context Edit;

	d_font Font;

	editable_buffer Buffer;
	cursor Cursor;
}
mayday_context;

#include "shaders.h"

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = OS_WindowHeight(&M->Window);

	v2f BufferPosition = v2f(200, (f32)WindowHeight - 100);

	v2f NumbersPosition = v2f(10, (f32)WindowHeight - 100);

	f32 LineHeight = D_LineHeightFromFont(M->Font, M->Buffer.HeightInPixels);

	for(int I = 0; I < 50; I++)
	{
		D_DrawStr8(&M->Draw,
			M->FrameArena, M->Font,
			BufferPosition, v4f(0.5, 0.1, 0, 1),
			50, Str8PushF(M->FrameArena, "?u32", I));
		NumbersPosition.Y -= LineHeight;
	}

	M->Buffer.HeightInPixels = 50;

	M->Buffer.String = (str8){sizeof(G_TriangleShader), G_TriangleShader};

	M->Cursor.TargetPosition = E_GetPositionFromEditableBuffer(&M->Draw,
															   &M->Cursor,
															   M->Buffer,
															   M->Font,
															   BufferPosition);
	M->Cursor.Position = LerpV2f(M->Cursor.Position, M->Cursor.TargetPosition, 0.3);


	D_DrawQuad(&M->Draw, M->FrameArena,
	M->Cursor.Position, 5, LineHeight, v4f(0, 1, 0, 1));


	D_DrawStr8(&M->Draw,
			   M->FrameArena, M->Font,
			   BufferPosition, v4f(1, 0.5, 0, 1),
			   50, M->Buffer.String);

	D_GlyphsToAtlas(&M->Draw);
	R_TextureFromBytes(&M->Render, M->Draw.FontAtlas, D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);

	R_Update(&M->Render);

	M->FrameArena->At = 0;

	ArenaReset(M->FrameArena);
}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday_context *M = (mayday_context*)Data;
	if(Event->type == SAPP_EVENTTYPE_KEY_DOWN)
	{	

		if(Event->key_code == SAPP_KEYCODE_BACKSPACE)	
		{
			E_RemoveTextFromTextRange(&M->Edit, M->Cursor, &M->Buffer,
							   M->Cursor.At-1, M->Cursor.At);
			M->Cursor.At -= 1;
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			M->Cursor.At += 1;
		}

		if(Event->key_code == SAPP_KEYCODE_LEFT)
		{
			M->Cursor.At -= 1;
		}

		if(Event->key_code == SAPP_KEYCODE_DOWN)
		{
			int DistanceFromNewLine = E_DistanceFromNextNewLine(&M->Draw, &M->Cursor, M->Buffer);
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&M->Draw, &M->Cursor, M->Buffer);
			if(M->Buffer.String.Bytes[M->Cursor.At] == '\n') M->Cursor.At++;
			M->Cursor.At += DistanceFromNewLine;
			M->Cursor.At += DistanceFromPreviousNewLine;
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&M->Draw, &M->Cursor, M->Buffer);
			int DistanceFromNewLine = E_DistanceFromNextNewLine(&M->Draw, &M->Cursor, M->Buffer);
			M->Cursor.At -= DistanceFromPreviousNewLine;
			M->Cursor.At -= 1;
			M->Cursor.At -= E_DistanceFromPreviousNewLine(&M->Draw, &M->Cursor, M->Buffer);
			M->Cursor.At += DistanceFromPreviousNewLine;
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
			E_InsertTextAtCursor(&M->Edit, M->Cursor, &M->Buffer, Str8("\n"));
			M->Cursor.At += 1;
		}
		else
		{
			E_InsertTextAtCursor(&M->Edit, M->Cursor, &M->Buffer, Str8PushF(M->FrameArena, "?char", Event->char_code));
			M->Cursor.At += 1;
		}
	}
}

void OS_Initialise(void *Data)
{
	mayday_context *M = Data;
	R_Init(&M->Render);
	//Application->R.Logger->Silent = True;
}

int main()
{
	memory_arena Arena = {0};
	memory_arena FrameArena = {0};

	logger Logger = {0};
	Logger.Silent = True;
	Logger.Arena = &Arena;

	mayday_context M = {0};
	
	M.PermanentArena = &Arena;
	M.FrameArena = &FrameArena;

	M.Render.Arena = &Arena;
	M.Render.Logger = &Logger;


	M.Draw.Arena = &Arena;
	M.Draw.Logger = &Logger;
	M.Draw.R = &M.Render;

	M.Window = OS_MakeWindow(Str8("Mayday"), 3024, 1964);

	M.Font = D_LoadFontFromTTF(&M.Draw, G_BerkeleyMonoRegular);

	OS_Run(M.Window, &M);
}