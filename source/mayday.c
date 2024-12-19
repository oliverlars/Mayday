
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
			   M->Buffer.HeightInPixels, M->Buffer.String);

	D_GlyphsToAtlas(&M->Draw);
	R_TextureFromBytes(&M->Render, M->Draw.FontAtlas, D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);

	R_Update(&M->Render, M->FrameArena);

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
			int BufferIndex = E_BufferIndexFromCursor(&M->Buffer, &M->Cursor);
			E_RemoveTextFromTextRange(&M->Edit, &M->Cursor, &M->Buffer, BufferIndex-1, BufferIndex);
			E_AdvanceCursorXByN(&M->Cursor, &M->Buffer, -1);
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			E_AdvanceCursorXByN(&M->Cursor, &M->Buffer, +1);
		}

		if(Event->key_code == SAPP_KEYCODE_EQUAL)
		{
			if(Event->modifiers == SAPP_MODIFIER_SUPER)
			{
				M->Buffer.HeightInPixels += 4;
			}
		}
		if(Event->key_code == SAPP_KEYCODE_MINUS)
		{
			if(Event->modifiers == SAPP_MODIFIER_SUPER)
			{
				M->Buffer.HeightInPixels -= 4;
			}
		}

		if(Event->key_code == SAPP_KEYCODE_LEFT)
		{
			
			E_AdvanceCursorXByN(&M->Cursor, &M->Buffer, -1);
		}

		if(Event->key_code == SAPP_KEYCODE_DOWN)
		{
			M->Cursor.Column += 1;
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			M->Cursor.Column -= 1;
		}

		if(Event->key_code == SAPP_KEYCODE_ENTER)
		{
			E_InsertTextAtCursor(&M->Edit, &M->Cursor, &M->Buffer, Str8("\n"));
		}

		if(Event->key_code == SAPP_KEYCODE_SPACE)
		{
			E_InsertTextAtCursor(&M->Edit, &M->Cursor, &M->Buffer, Str8(" "));
		}

		if(Event->key_code == SAPP_KEYCODE_TAB)
		{
			E_InsertTextAtCursor(&M->Edit, &M->Cursor, &M->Buffer, Str8("\t"));
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_CHAR && Event->char_code > 32 && Event->char_code <= 126)
	{
		if(!(Event->modifiers & SAPP_MODIFIER_SUPER))
		{
			E_InsertTextAtCursor(&M->Edit, &M->Cursor, &M->Buffer,
								 Str8PushF(M->FrameArena, "?char", Event->char_code));
		}
	}
}

void OS_Initialise(void *Data)
{
	mayday_context *M = Data;
	M->Buffer.HeightInPixels = 48;
	R_Init(&M->Render);
	//Application->R.Logger->Silent = True;
}

int main()
{
	memory_arena Arena = {0};
	memory_arena FrameArena = {0};

	logger Logger = {0};
	//Logger.Silent = True;
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