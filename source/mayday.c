
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

#include "fdg.h"

// TODO(Oliver): Remove platform specific code from here
#include "darwin/menu_bar.h"

#include "wave.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct mayday_context
{
	m_arena *FrameArena;
	m_arena *PermanentArena;


	render_context *Render;
	draw_context   *Draw;
	edit_context   *Edit;
	ui_context     *UI;

	os_window Window;

	d_font CodeFont;
	d_font LogoFont;

	editable_buffer Buffer;
	cursor Cursor;


	v2f CameraPosition;
	f32 ViewSize;
	f32 ViewSizeTarget;

	ui_tree TreeView;
	str8 Folder;

	shared_wave_info Shared;
}
mayday_context;

#include "shaders.h"

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	UI_Begin(M->UI);

	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);

	m44f Transform = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	R_PushTransform(M->Render, Transform);
	D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
	

	if(M->UI->View == UI_VIEW_EDIT)
	{
		UI_Buffer(M->UI, M->Draw, M->Edit,
			   M->FrameArena, M->CodeFont,
			   &M->Buffer,
			   &M->Cursor,
			   v2i(WindowWidth, WindowHeight));

		UI_StatusBar(M->UI, M->Draw, M->Edit, M->FrameArena, M->CodeFont,
					 &M->Cursor, &M->Buffer,
					 v2i(WindowWidth, WindowHeight));
	}
	else if(M->UI->View == UI_VIEW_TREE)
	{
		UI_TreeView(M->UI, M->Draw,  &M->Shared,
					M->FrameArena, &M->TreeView, M->Folder, v2f((f32)WindowWidth, (f32)WindowHeight));
		M->Folder = UI_TreeViewFolderLabels(M->UI, M->Draw, M->FrameArena, &M->TreeView, M->Folder);
	}

	R_PopTransform(M->Render);
	
	if(OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenMousePressed) > 150.0 &&
	   M->UI->MouseWasPressed && M->UI->MultiClickCount == 0)
	{
		M->UI->MouseIsDragging = True;
	}
	else if(!M->UI->MouseWasPressed)
	{
		M->UI->MouseIsDragging = False;
	}

	R_Update(M->Render, M->FrameArena);

	D_GlyphsToAtlas(M->Draw);

	ArenaReset(M->FrameArena);

	M->UI->MouseWasPressed = False;
	M->UI->MouseWentUp = False;
}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday_context *M = (mayday_context*)Data;

	if(Event->type == SAPP_EVENTTYPE_MOUSE_DOWN)
	{
		int WindowHeight = OS_WindowHeight(&M->Window);

		v2f BufferPosition = v2f(100, (f32)WindowHeight - 50);

		if(Event->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			int Index = E_IndexFromPosition(M->Edit, M->Draw, M->CodeFont,
										&M->Buffer, BufferPosition,
										M->UI->MousePosition);

			cursor *LastCursor = E_LastCursor(M->Edit, &M->Cursor);
			cursor *Cursor = LastCursor;
			if(Event->modifiers & SAPP_MODIFIER_ALT)
			{
				cursor NextCursor = Cursor[0];
				NextCursor.Selection = v2i(Index, Index);
				E_AppendCursor(M->Edit, M->PermanentArena, &M->Cursor, NextCursor);
				Cursor = Cursor->Next;
			}
			else
			{
				M->Cursor.Next = Null;
				Cursor = &M->Cursor;
			}

			f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenMousePressed);
			if(MillisecondsSinceLastClick < 400 &&
			   M->UI->MultiClickCount == 0)
			{
				v2i InitialSelection = Cursor->Selection;
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, Cursor, &M->Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
				v2i Selection = Cursor->Selection;
				Cursor->Selection = InitialSelection;
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, Cursor, &M->Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
				Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);
				M->UI->MultiClickCount++;
				
				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiClickCount == 1)
			{
				int LineStart = E_LineStartFromIndex(M->Edit, Cursor->Selection.Y, &M->Buffer);
				int LineEnd = E_LineEndFromIndex(M->Edit, Cursor->Selection.Y, &M->Buffer);
				Cursor->Selection = v2i(LineStart, LineEnd);
				M->UI->MultiClickCount++;
				
				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiClickCount == 2)
			{
				Cursor->Selection = v2i(0, M->Buffer.String.Size);
				M->UI->MultiClickCount++;

				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick >= 400)
			{
				Cursor->Selection = v2i(Index, Index);	
				M->UI->MultiClickCount = 0;
			}

			M->UI->MouseWasPressed = True;
			M->UI->MouseWentUp = False;
			M->UI->TimeStampWhenMousePressed = OS_TimeStampNow();
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_SCROLL)
	{
		M->UI->ScrollOffset.Y -= Event->scroll_y;
		M->UI->ScrollOffset.X += Event->scroll_x;
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_UP)
	{
		M->UI->MouseWentUp = True;
		M->UI->MouseWasPressed = False;
	}
	else if(Event->type == SAPP_EVENTTYPE_KEY_DOWN)
	{
		edit_move_type MoveType = E_MOVE_NOT_SELECTING;
		if(Event->modifiers & SAPP_MODIFIER_SHIFT)
		{
			MoveType = E_MOVE_SELECTING;
		}

		if(Event->key_code == SAPP_KEYCODE_ESCAPE)
		{
			// HACK(Oliver): This will mean multiple cursors leak memory
			M->Cursor.Next = Null;
		}

		if(Event->key_code == SAPP_KEYCODE_BACKSPACE)	
		{
			E_DeleteTo(M->Edit, &M->Cursor, &M->Buffer);

		}

		if(Event->key_code == SAPP_KEYCODE_Z)
		{
		}

		if(Event->key_code == SAPP_KEYCODE_RIGHT)
		{
			if(Event->modifiers & SAPP_MODIFIER_SUPER)
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_LINE_END, MoveType);
			}
			else if(Event->modifiers & SAPP_MODIFIER_ALT)
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_WORD_RIGHT, MoveType);
			}
			else
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_RIGHT, MoveType);
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
			if(Event->modifiers & SAPP_MODIFIER_SUPER)
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_LINE_START, MoveType);
			}
			else if(Event->modifiers & SAPP_MODIFIER_ALT)
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_WORD_LEFT, MoveType);
			}
			else
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_LEFT, MoveType);
			}
		}

		if(Event->key_code == SAPP_KEYCODE_DOWN)
		{
			if(Event->modifiers & SAPP_MODIFIER_ALT)
			{
				cursor *LastCursor = E_LastCursor(M->Edit, &M->Cursor);
				cursor NextCursor = LastCursor[0];
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &NextCursor, &M->Buffer, E_MOVE_DOWN, MoveType);
				E_AppendCursor(M->Edit, M->PermanentArena, &M->Cursor, NextCursor);
			}
			else
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_DOWN, MoveType);
			}
		}

		if(Event->key_code == SAPP_KEYCODE_UP)
		{
			if(Event->modifiers & SAPP_MODIFIER_ALT)
			{
				cursor *LastCursor = E_LastCursor(M->Edit, &M->Cursor);
				cursor NextCursor = LastCursor[0];
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &NextCursor, &M->Buffer, E_MOVE_UP, MoveType);
				E_AppendCursor(M->Edit, M->PermanentArena, &M->Cursor, NextCursor);
			}
			else
			{
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, &M->Cursor, &M->Buffer, E_MOVE_UP, MoveType);
			}
		}

		if(Event->key_code == SAPP_KEYCODE_ENTER)
		{
			E_InsertTo(M->Edit, &M->Cursor, &M->Buffer, Str8("\n"));

			int TotalWhiteSpaceInserted = 0;
			for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
			{
				int LineStart = E_LineStartFromIndex(M->Edit, Cursor->Selection.Y, &M->Buffer);
				
				int FirstNonWhitespace = LineStart;
				for(;M->Buffer.String.Bytes[FirstNonWhitespace] == ' ' ||
					 M->Buffer.String.Bytes[FirstNonWhitespace] == '\t';)
				{
					FirstNonWhitespace++;
				}
				str8 WhiteSpaceToInsert = (str8){	FirstNonWhitespace - LineStart, M->Buffer.String.Bytes + LineStart};

				E_InsertTextAtIndex(M->Edit, Cursor->Selection.Y, &M->Buffer, WhiteSpaceToInsert);
				TotalWhiteSpaceInserted += WhiteSpaceToInsert.Size;
				Cursor->Selection.Y += TotalWhiteSpaceInserted;
				Cursor->Selection.X = Cursor->Selection.Y;
			}
		}

		if(Event->key_code == SAPP_KEYCODE_SPACE)
		{
			E_InsertTo(M->Edit, &M->Cursor, &M->Buffer, Str8(" "));
		}

		if(Event->key_code == SAPP_KEYCODE_TAB)
		{
			E_InsertTo(M->Edit, &M->Cursor, &M->Buffer, Str8("\t"));
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_CHAR && Event->char_code > 32 && Event->char_code <= 126)
	{
		E_InsertTo(M->Edit, &M->Cursor, &M->Buffer, Str8PushF(M->FrameArena, "?char", Event->char_code));
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_MOVE)
	{
		M->UI->MousePosition.X = Event->mouse_x;
		M->UI->MousePosition.Y = OS_WindowHeight(&M->Window) - Event->mouse_y;
	}
}

void OS_Initialise(void *Data)
{
	mayday_context *M = Data;
	M->Buffer.HeightInPixels = 48;
	R_Init(M->Render, M->PermanentArena, v2i(sapp_width() , sapp_height()));

	M->Buffer.String.Bytes = ArenaPushZero(M->PermanentArena, MegaBytes(1));
	M->Buffer.String.Size = sizeof(G_TriangleShader);
	MemoryCopy(M->Buffer.String.Bytes, G_TriangleShader, sizeof(G_TriangleShader));

	DarwinMenuBar();
}

int main()
{
	m_arena *Arena = ArenaInitialise(GigaBytes(64));
	m_arena *FrameArena = ArenaInitialise(GigaBytes(1));

	logger Logger = {0};
	Logger.Arena = ArenaInitialise(GigaBytes(1));
	per_thread_context *ThreadContext = GetThreadContext();
	ThreadContext->Logger = &Logger;
	ThreadContext->ScratchArenas[0] = ArenaInitialise(GigaBytes(1));
	ThreadContext->ScratchArenas[1] = ArenaInitialise(GigaBytes(1));

	mayday_context M = {0};

	M.PermanentArena = Arena;
	M.FrameArena = FrameArena;

	M.Draw = ArenaPushArrayZero(Arena, draw_context, 1);
	M.Render = ArenaPushArrayZero(Arena, render_context, 1);
	M.Edit = ArenaPushArrayZero(Arena, edit_context, 1);
	M.UI = ArenaPushArrayZero(Arena, ui_context, 1);
	M.Draw->Render = M.Render;
	M.Draw->Arena = Arena;
	M.Edit->Arena = Arena;
	M.Edit->Temp = ArenaPushZero(Arena, MegaBytes(1));

	M.Window = OS_MakeWindow(Str8("Mayday!"), 3024, 1964);

	M.CodeFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular);
	M.LogoFont = D_LoadFontFromTTF(M.Draw, G_BasementGrotesque);

	M.UI->Font = M.CodeFont;

	M.CameraPosition = v2f(0, 0);
	M.ViewSize = 5.0;
	M.ViewSizeTarget = M.ViewSize;

	M.Shared.Arena = M.PermanentArena;

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 16);

	M.Folder = Str8("/Users/olivercruickshank/EmberGen/liquigen");

	OS_Run(M.Window, &M);
}