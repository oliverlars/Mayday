
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

#include "ui.h"
#include <assert.h>

typedef struct mayday_context
{
	memory_arena *FrameArena;
	memory_arena *PermanentArena;
	os_window Window;
	render_context Render;
	draw_context Draw;
	edit_context Edit;

	ui_context UI;

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
			v2f(50, WindowHeight - 70), v4f(1, 1, 1, 1),
			M->Buffer.HeightInPixels, Str8PushF(M->FrameArena,
												"Line: ?u32 | StickyX: ?u32 | Start: ?u32 | End: ?u32 | VisualX: ?u32 | DistanceFromStart: ?u32 | DistanceFromEnd: ?u32",
												E_LineFromBufferIndex(&M->Edit, &M->Cursor, &M->Buffer),
												0,
												M->Cursor.Start,
												M->Cursor.End,
												M->Cursor.VisualX,
												E_DistanceFromPreviousNewLine(&M->Edit, &M->Cursor, &M->Buffer),
												E_DistanceFromNextNewLine(&M->Edit, &M->Cursor, &M->Buffer)));

	f32 HeightOffset = 0;
	for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
	{
		D_DrawStr8(&M->Draw, M->FrameArena, M->Font, v2f(1600, WindowHeight - 500 + HeightOffset), v4f(1, 1, 1, 1),
			M->Buffer.HeightInPixels, Str8PushF(M->FrameArena, "Start: ?u32 | End: ?u32 | VisualX: ?u32",
			Cursor->Start, Cursor->End, Cursor->VisualX));
		HeightOffset -= M->Buffer.HeightInPixels;
	}

	UI_TextButton(&M->UI, &M->Draw, M->FrameArena, Str8("Test Button"), 80, v2f(1600, 800));

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

	for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
	{
		int Start = Cursor->Start;
		int End = Cursor->End;

		//if(Start == End) continue;

		if(Cursor->End < Cursor->Start)
		{
			int Temp = Start;
			Start = End;
			End = Temp;
		}
		D_DrawSelectionRange(&M->Draw,
							M->FrameArena,
							M->Font,
							BufferPosition,
							v4f(0.1, 0.05, 0, 0.1),
							M->Buffer.HeightInPixels,
							M->Buffer.String,
							Start,
							End);
		
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
			
			if(M->Cursor.Start < M->Cursor.End)
			{
				PushUndoAction(&M->Edit, &M->Cursor, UNDO_REMOVED, 
				(str8){M->Cursor.End - M->Cursor.Start, M->Buffer.String.Bytes + M->Cursor.Start});
			}
			else
			{
				PushUndoAction(&M->Edit, &M->Cursor, UNDO_REMOVED, 
				(str8){M->Cursor.Start - M->Cursor.End, M->Buffer.String.Bytes + M->Cursor.End});
			}

			E_RemoveSelectedTextAtCursors(&M->Edit, &M->Cursor, &M->Buffer);	
		}

		if(Event->key_code == SAPP_KEYCODE_Z)
		{
			if(Event->modifiers & SAPP_MODIFIER_SUPER)
			{
				undo_stack *Top = PopOffUndoStack(&M->Edit);
				cursor Cursor = {0};
				Cursor.End = Top->Index;
				Cursor.Start = Cursor.End;
				E_InsertTextAtCursor(&M->Edit, &Cursor, &M->Buffer, Top->String);
				
			}
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			E_AdvanceCursorsXByN(&M->Edit, &M->Cursor, &M->Buffer, +1);	
			if(!(Event->modifiers & SAPP_MODIFIER_SHIFT))
			{
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					Cursor->Start = Cursor->End;
				}
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
			E_AdvanceCursorsXByN(&M->Edit, &M->Cursor, &M->Buffer, -1);
			if(!(Event->modifiers & SAPP_MODIFIER_SHIFT))
			{
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					Cursor->Start = Cursor->End;
				}
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
				E_AdvanceCursorsYByN(&M->Edit, &M->Draw, M->Font, LastCursor->Next, &M->Buffer, 1);
			}
			else
			{
				E_AdvanceCursorsYByN(&M->Edit, &M->Draw, M->Font, &M->Cursor, &M->Buffer, 1);
			}

			if(!(Event->modifiers & SAPP_MODIFIER_SHIFT))
			{
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					Cursor->Start = Cursor->End;
				}
			}	
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			E_AdvanceCursorsYByN(&M->Edit, &M->Draw, M->Font, &M->Cursor, &M->Buffer, -1);

			if(!(Event->modifiers & SAPP_MODIFIER_SHIFT))
			{
				for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
				{
					Cursor->Start = Cursor->End;
				}
			}	
		}

		if(Event->key_code == SAPP_KEYCODE_ENTER)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				if(Cursor->Start != Cursor->End)
				{
					E_RemoveSelectedTextAtCursors(&M->Edit, &M->Cursor, &M->Buffer);
				}
			}
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8("\n"));
			}
		}

		if(Event->key_code == SAPP_KEYCODE_SPACE)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				if(Cursor->Start != Cursor->End)
				{
					E_RemoveSelectedTextAtCursors(&M->Edit, &M->Cursor, &M->Buffer);
				}
			}

			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8(" "));
			}
		}

		if(Event->key_code == SAPP_KEYCODE_TAB)
		{
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				if(Cursor->Start != Cursor->End)
				{
					E_RemoveSelectedTextAtCursors(&M->Edit, &M->Cursor, &M->Buffer);
				}
			}

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
				if(Cursor->Start != Cursor->End)
				{
					E_RemoveSelectedTextAtCursors(&M->Edit, &M->Cursor, &M->Buffer);
				}
			}

			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				E_InsertTextAtCursor(&M->Edit, Cursor, &M->Buffer, Str8PushF(M->FrameArena, "?char", Event->char_code));
			}
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_MOVE)
	{
		M->UI.MousePosition.X = Event->mouse_x;
		M->UI.MousePosition.Y = OS_WindowHeight(&M->Window) - Event->mouse_y;
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

	M.UI.Font = M.Font;

	OS_Run(M.Window, &M);
}