
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
	m_arena *PreviousFrameArena;

	m_arena *PermanentArena;


	render_context *Render;
	draw_context   *Draw;
	edit_context   *Edit;
	ui_context     *UI;

	os_window Window;

	d_font CodeFont;
	d_font LogoFont;
	d_font StatusFont;

	editable_buffer Buffer;
	cursor Cursor;


	editable_buffer TextBoxBuffer;


	v3f CameraPosition;
	f32 ViewSize;
	f32 ViewSizeTarget;

	int LookAtNodeIndex;
	v3f LookAtTarget;
	v3f LookAt;

	v3f LookFromTarget;
	v3f LookFrom;

	ui_tree TreeView;
	int TreeDepth;
	int TreeParentIndex;


	str8 Folder;

	shared_wave_info Shared;
}
mayday_context;

#include "shaders.h"

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));

	UI_Begin(M->UI);

	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);

	f32 Size = M->ViewSize;
	static f32 Angle = 0.0;
	Angle += 0.001 / (f32)(M->TreeDepth*2 + 1);
	m44f Rotation = M44FromYAngle(Angle);
	m44f Transform = M44FOrthographic(-Size, +Size, +Size, -Size, -1000.0, 1000.0);
	//m44f Transform = M44FPerspective(sapp_widthf()/sapp_heightf(), 70, 0.01, 10000.0);
	M->LookAt = LerpV3F(M->LookAt, M->LookAtTarget, 0.1);
	M->ViewSize = LerpF32(M->ViewSize, M->ViewSizeTarget, 0.1);
	if(M->TreeView.Info.NodeInfos)
	{
		bounds Bounds = GetBounds(M->TreeView.Info.NodeTargetPositions, M->TreeView.Info.NodeCount);
		v4f P = M->TreeView.Info.NodePositions[M->LookAtNodeIndex];
		M->LookAtTarget = v3f(P.X, P.Y, P.Z);

		f32 LongestSide = Max((Bounds.Max.X - Bounds.Min.X), Max((Bounds.Max.Z - Bounds.Min.Z), (Bounds.Max.Y - Bounds.Min.Y)));

		M->ViewSizeTarget = LongestSide/((f32)(M->TreeDepth)*3.0 + 1.5);

		Transform = MulM44FM44F(M44FLookAt(v3f(0, 0, 2.0*LongestSide), M->LookAt, v3f(0, 1, 0)), Transform);
	}
	else
	{
		Transform = MulM44FM44F(M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0)), Transform);
	}


	Transform = MulM44FM44F(M44FTranslate(M->LookAt), Transform);
	Transform = MulM44FM44F(Rotation, Transform);
	Transform = MulM44FM44F(M44FTranslate(MulV3F(M->LookAt, v3f(-1,-1,-1))), Transform);
	R_PushTransform(M->Render, Transform);

	if(M->UI->View == UI_VIEW_EDIT)
	{
		u64 Start = OS_TimeStampNow();
		R_PushTransform(M->Render, M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		D_DrawGrid2D(M->Draw, M->FrameArena, 50, 200);
		UI_Buffer(M->UI, M->Draw, M->Edit,
			   M->FrameArena, M->CodeFont,
			   &M->Buffer,
			   v2i(WindowWidth, WindowHeight));

		UI_StatusBar(M->UI, M->Draw, M->Edit, M->FrameArena, M->StatusFont,
					 &M->Cursor, &M->Buffer,
					 v2i(WindowWidth, WindowHeight));

		u64 End = OS_TimeStampNow();
		//LogInfo("Time: ?f32", OS_MilliSecondsFromTimeStamp(End - Start));
		R_PopTransform(M->Render);
	}
	else if(M->UI->View == UI_VIEW_TREE)
	{
		v2f Centre = {0};
		R_PushTransform(M->Render, M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		D_DrawGrid2D(M->Draw, M->FrameArena, 50, 200);
		R_PopTransform(M->Render);

		if(M->TreeView.Info.NodePositions)
		{
			//Centre = SubV3F(v3f(sapp_widthf()/2.0, sapp_heightf()/2.0), M->TreeView.Info.Nodes[0].Position);
		}

		UI_TreeView(M->UI, M->Draw,  &M->Shared,
					M->FrameArena, &M->TreeView, M->Folder, v3f(0.1, 0.1, 0.1));

		R_PushTransform(M->Render, M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));

		ui_tree_view_signal TreeSignal = UI_TreeViewFolderLabels(M->UI, M->Draw, M->Edit, M->StatusFont, M->FrameArena,
																 M->LookAtNodeIndex, &M->TreeView,
																 Transform, M->Folder, &M->TextBoxBuffer);

		if(!OS_ValidPath(M->Folder))
		{
			f32 CentreX = sapp_widthf()/2.0 - UI_GetTextWidth(M->LogoFont, M->Draw, Str8("MAYDAY!"), 200)/2.0;
			D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, v2f(CentreX, sapp_heightf()/1.5), v4f(0.8, 0.05, 0.0, 1.0), 200, Str8("MAYDAY!"));
			ui_textbox_signal SearchSignal = UI_TextBox(M->FrameArena, M->StatusFont, v2f(sapp_widthf()/2.0 - 500, 100), 1000,
														M->UI, M->Draw, M->Edit, 60, Str8("FolderTextBox"), &M->TextBoxBuffer);
			if(SearchSignal.Submitted)
			{
				// HACK(Oliver): This is leaking memory
				M->Folder = Str8Copy(M->PermanentArena, SearchSignal.String);
				M->UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
			}
		}

		if(TreeSignal.Clicked)
		{
			M->LookAtNodeIndex = TreeSignal.NodeIndex;
			M->TreeDepth = TreeSignal.NodeDepth;
			if(TreeSignal.FileToLoad.Size > 0)
			{
				M->Buffer.String = OS_LoadEntireFileIntoMemory(M->PermanentArena, TreeSignal.FileToLoad);
				M->UI->View = UI_VIEW_EDIT;
			}
		}

		//D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
		R_PopTransform(M->Render);
	}

	R_PopTransform(M->Render);

	if(OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed) > 150.0 &&
	   M->UI->LeftMouseIsDown)
	{
		M->UI->MouseIsDragging = True;
	}

	if(!M->UI->LeftMouseIsDown)
	{
		M->UI->MouseIsDragging = False;
	}

	R_Update(M->Render, M->FrameArena);

	D_GlyphsToAtlas(M->Draw);

	ArenaReset(M->FrameArena);
	m_arena *TempArena = M->FrameArena;
	M->FrameArena = M->PreviousFrameArena;
	M->PreviousFrameArena = TempArena;

	M->UI->EventLast = Null;
	M->UI->EventFirst = Null;

	f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed);
	if(MillisecondsSinceLastClick >= 400)
	{
		M->UI->MultiLeftClickCount = 0;
	}

}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday_context *M = (mayday_context*)Data;

	{
		ui_event *UIEvent = ArenaPushArrayZero(M->FrameArena, ui_event, 1);
		UIEvent->E = Event[0];
		if(M->UI->EventLast)
		{
			M->UI->EventLast->Next = UIEvent;
			UIEvent->Previous = M->UI->EventLast;
			M->UI->EventLast = UIEvent;
		}
		else
		{
			M->UI->EventLast = UIEvent;
			M->UI->EventFirst = UIEvent;
		}
	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_MOVE)
	{
		M->UI->MousePosition.X = Event->mouse_x;
		M->UI->MousePosition.Y = OS_WindowHeight(&M->Window) - Event->mouse_y;
	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_DOWN)
	{
		if(Event->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			M->UI->LeftMouseIsDown = True;

			f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed);

			if(MillisecondsSinceLastClick < 400 &&
			   M->UI->MultiLeftClickCount == 0)
			{
				M->UI->MultiLeftClickCount += 1;
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 1)
			{
				M->UI->MultiLeftClickCount += 1;
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 2)
			{
				M->UI->MultiLeftClickCount += 1;
			}

			M->UI->TimeStampWhenLeftMousePressed = OS_TimeStampNow();
		}

	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_UP)
	{
		if(Event->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			M->UI->LeftMouseIsDown = False;
		}
	}

	return;

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

			f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed);
			if(MillisecondsSinceLastClick < 400 &&
			   M->UI->MultiLeftClickCount == 0)
			{
				v2i InitialSelection = Cursor->Selection;
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, Cursor, &M->Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
				v2i Selection = Cursor->Selection;
				Cursor->Selection = InitialSelection;
				E_MoveCursors(M->Edit, M->Draw, M->CodeFont, Cursor, &M->Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
				Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);
				M->UI->MultiLeftClickCount++;

				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 1)
			{
				int LineStart = E_LineStartFromIndex(M->Edit, Cursor->Selection.Y, &M->Buffer);
				int LineEnd = E_LineEndFromIndex(M->Edit, Cursor->Selection.Y, &M->Buffer);
				Cursor->Selection = v2i(LineStart, LineEnd);
				M->UI->MultiLeftClickCount++;

				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 2)
			{
				Cursor->Selection = v2i(0, M->Buffer.String.Size);
				M->UI->MultiLeftClickCount++;

				if(LastCursor != Cursor)
				{
					LastCursor[0] = Cursor[0];
					LastCursor->Next = Null;
				}
			}
			else if(MillisecondsSinceLastClick >= 400)
			{
				Cursor->Selection = v2i(Index, Index);
				M->UI->MultiLeftClickCount = 0;
			}

			M->UI->TimeStampWhenLeftMousePressed = OS_TimeStampNow();
		}
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_SCROLL)
	{
		M->UI->ScrollOffset.Y -= Event->scroll_y;
		M->UI->ScrollOffset.X += Event->scroll_x;
	}
	else if(Event->type == SAPP_EVENTTYPE_MOUSE_UP)
	{

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

	M->TextBoxBuffer.String.Bytes = ArenaPushArrayZero(M->PermanentArena, u8, 8192);
	M->TextBoxBuffer.HeightInPixels = 60;

	M->UI->Active =  UI_IDFromString(Str8("FolderTextBox"));

	DarwinMenuBar();
}

int main()
{
	m_arena *Arena = ArenaInitialise(GigaBytes(64));
	m_arena *FrameArena = ArenaInitialise(GigaBytes(1));
	m_arena *PreviousFrameArena = ArenaInitialise(GigaBytes(1));

	logger Logger = {0};
	Logger.Arena = ArenaInitialise(GigaBytes(1));
	per_thread_context *ThreadContext = GetThreadContext();
	ThreadContext->Logger = &Logger;
	ThreadContext->ScratchArenas[0] = ArenaInitialise(GigaBytes(1));
	ThreadContext->ScratchArenas[1] = ArenaInitialise(GigaBytes(1));

	mayday_context M = {0};

	M.PermanentArena = Arena;
	M.FrameArena = FrameArena;
	M.PreviousFrameArena = PreviousFrameArena;

	M.Draw = ArenaPushArrayZero(Arena, draw_context, 1);
	M.Render = ArenaPushArrayZero(Arena, render_context, 1);
	M.Edit = ArenaPushArrayZero(Arena, edit_context, 1);
	M.UI = ArenaPushArrayZero(Arena, ui_context, 1);
	M.Draw->Render = M.Render;
	M.Draw->Arena = Arena;
	M.Edit->Arena = Arena;
	M.Edit->Temp = ArenaPushZero(Arena, MegaBytes(1));

	cursor Cursor = {0};
	M.Edit->Cursors = &Cursor;

	cursor Cursor2 = {0};
	M.TextBoxBuffer.Cursors = &Cursor2;

	M.Window = OS_MakeWindow(Str8("Mayday!"), 3024, 1964);

	M.CodeFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular);
	M.LogoFont = D_LoadFontFromTTF(M.Draw, G_BasementGrotesque);
	M.StatusFont = D_LoadFontFromTTF(M.Draw, G_Doto);

	M.UI->Font = M.CodeFont;

	M.CameraPosition = v3f(0.0);
	M.ViewSize = 40.0;
	M.ViewSizeTarget = M.ViewSize;

	M.Shared.Arena = ArenaInitialise(GigaBytes(1));

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 15);

	M.Folder = Str8("");

	OS_Run(M.Window, &M);
}
