
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
#include <assert.h>

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
	int WindowWidth = OS_WindowWidth(&M->Window);

	v2f BufferPosition = v2f(100, (f32)WindowHeight - 150);

	v2f NumbersPosition = v2f(10, (f32)WindowHeight - 150);

	f32 LineHeight = D_LineHeightFromFont(M->Font, M->Buffer.HeightInPixels);

	D_DrawStr8(&M->Draw,
			M->FrameArena, M->Font,
			v2f(700, WindowHeight - 100), v4f(1, 1, 1, 1),
			M->Buffer.HeightInPixels, Str8PushF(M->FrameArena,
												"Line: ?u32 | StickyX: ?u32 | BufferIndex: ?u32 | VisualX: ?u32 | DistanceFromStart: ?u32 | DistanceFromEnd: ?u32",
												E_LineFromBufferIndex(&M->Edit, &M->Cursor, &M->Buffer),
												0,
												E_BufferIndexFromCursor(&M->Edit, &M->Buffer, &M->Cursor),
												M->Cursor.VisualX,
												E_DistanceFromPreviousNewLine(&M->Edit, &M->Cursor, M->Buffer),
												E_DistanceFromNextNewLine(&M->Edit, &M->Cursor, M->Buffer)));


	for(int I = 0; I < 50; I++)
	{
		if(I == 0)
		{
			D_DrawStr8(&M->Draw, M->FrameArena, M->Font, NumbersPosition,
				v4f(1.0, 0.5, .2, 1), LineHeight, Str8PushF(M->FrameArena, "?u32", I));
		}
		else
		{
			D_DrawStr8(&M->Draw, M->FrameArena, M->Font, NumbersPosition,
					v4f(0.2, 0.1, 0, 1), LineHeight, Str8PushF(M->FrameArena, "?u32", I));
		}
		NumbersPosition.Y -= LineHeight;
	}

	D_DrawStr8(&M->Draw,
			M->FrameArena, M->Font,
			BufferPosition, v4f(1, 0.5, 0, 1),
			M->Buffer.HeightInPixels, M->Buffer.String);


	for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
	{
		Cursor->TargetPosition = E_GetPositionFromEditableBuffer(&M->Edit,
																   &M->Draw,
																   Cursor,
																   M->Buffer,
																   M->Font,
																   BufferPosition);

		Cursor->Position = LerpV2f(Cursor->Position, Cursor->TargetPosition, 0.3);

		D_DrawQuad(&M->Draw, M->FrameArena, Cursor->Position, 5, LineHeight, v4f(0, 1, 0, 1));
	}



	D_GlyphsToAtlas(&M->Draw);

	R_Update(&M->Render, M->FrameArena);

	ArenaReset(M->FrameArena);
}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday_context *M = (mayday_context*)Data;
	if(Event->type == SAPP_EVENTTYPE_KEY_DOWN)
	{	

		if(Event->key_code == SAPP_KEYCODE_ESCAPE)
		{
			// HACK(Oliver): This will mean multiple cursors leak memory
			M->Cursor.Next = Null;
		}

		if(Event->key_code == SAPP_KEYCODE_BACKSPACE)	
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				int BufferIndex = E_BufferIndexFromCursor(&M->Edit, &M->Buffer, Cursor);
				E_RemoveTextFromTextRange(&M->Edit, Cursor, &M->Buffer, BufferIndex-1, BufferIndex);
			}
			//assert(BufferIndex == E_BufferIndexFromCursor(&M->Edit, &M->Buffer, &M->Cursor));
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				Cursor->VisualX = 0;
				E_AdvanceCursorXByN(&M->Edit, Cursor, &M->Buffer, +1);	
			}
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
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				Cursor->VisualX = 0;
				E_AdvanceCursorXByN(&M->Edit, Cursor, &M->Buffer, -1);
			}
		}

		if(Event->key_code == SAPP_KEYCODE_DOWN)
		{
			if(Event->modifiers & SAPP_KEYCODE_LEFT_ALT)
			{
				cursor NextCursor = {0};
				cursor *LastCursor = &M->Cursor;
				for(;LastCursor->Next; LastCursor = LastCursor->Next);
				
				NextCursor = LastCursor[0];
				E_AppendCursor(&M->Edit, LastCursor, NextCursor);
			}
			else
			{
				int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&M->Edit, &M->Cursor, M->Buffer);
				int DistanceFromNextNewLine = E_DistanceFromNextNewLine(&M->Edit, &M->Cursor, M->Buffer);
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					
					if(Cursor->VisualX == 0)
					{
						int RangeStart = Cursor->End - DistanceFromPreviousNewLine;
						int RangeEnd = Cursor->End;
						Cursor->VisualX = E_GetMaxTextWidthFromRange(&M->Edit, &M->Draw, M->Font, Cursor, &M->Buffer, RangeStart, RangeEnd);
					}
					Cursor->End += DistanceFromNextNewLine + 1;
					DistanceFromNextNewLine = E_DistanceFromNextNewLine(&M->Edit, &M->Cursor, M->Buffer);

					int RangeEndFromVisualX = E_RangeEndFromWidth(&M->Edit, &M->Draw, M->Font, Cursor, &M->Buffer, Cursor->VisualX, Cursor->End);
					Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);
				}
			}
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&M->Edit, &M->Cursor, M->Buffer);
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					if(Cursor->VisualX == 0)
					{
						int RangeStart = Cursor->End - DistanceFromPreviousNewLine;
						int RangeEnd = Cursor->End;
						Cursor->VisualX = E_GetMaxTextWidthFromRange(&M->Edit, &M->Draw, M->Font, Cursor, &M->Buffer, RangeStart, RangeEnd);
					}
					Cursor->End -= DistanceFromPreviousNewLine + 1;
					DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(&M->Edit, &M->Cursor, M->Buffer);
					
					Cursor->End -= DistanceFromPreviousNewLine;
					int DistanceFromNextNewLine = E_DistanceFromNextNewLine(&M->Edit, &M->Cursor, M->Buffer);

					int RangeEndFromVisualX = E_RangeEndFromWidth(&M->Edit, &M->Draw, M->Font, Cursor, &M->Buffer, Cursor->VisualX, Cursor->End);
					Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);
				}	
			}
		}

		if(Event->key_code == SAPP_KEYCODE_ENTER)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8("\n"));
			}
		}

		if(Event->key_code == SAPP_KEYCODE_SPACE)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8(" "));
			}
		}

		if(Event->key_code == SAPP_KEYCODE_TAB)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8("\t"));
			}
			
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_CHAR && Event->char_code > 32 && Event->char_code <= 126)
	{
		if(!(Event->modifiers & SAPP_MODIFIER_SUPER))
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer,
								 Str8PushF(M->FrameArena, "?char", Event->char_code));
			}
		}
	}
}

void OS_Initialise(void *Data)
{
	mayday_context *M = Data;
	M->Buffer.HeightInPixels = 48;
	R_Init(&M->Render);

	M->Buffer.String.Bytes = ArenaPushZero(M->PermanentArena, MegaBytes(1));
	M->Buffer.String.Size = sizeof(G_TriangleShader);
	MemoryCopy(M->Buffer.String.Bytes, G_TriangleShader, sizeof(G_TriangleShader));
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

	M.Edit.Logger = &Logger;

	M.Window = OS_MakeWindow(Str8("Mayday"), 3024, 1964);

	M.Font = D_LoadFontFromTTF(&M.Draw, G_BerkeleyMonoRegular);

	OS_Run(M.Window, &M);
}