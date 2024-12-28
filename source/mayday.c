
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

#include <assert.h>
#include <math.h>
#include <stdint.h>

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

	fdg_node *Nodes;
	int NodeCount;

	fdg_link *Links;
	int LinkCount;
}
mayday_context;

#include "shaders.h"



void EditingWindow(mayday_context *M)
{

	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);
	D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.009, 0.00, 0, 1.0));

	v2f BufferPosition = v2f(100, (f32)WindowHeight - 50 + M->UI->ScrollOffset.Y*15);

	v2f NumbersPosition = v2f(10, (f32)WindowHeight - 50 + M->UI->ScrollOffset.Y*15);

	f32 LineHeight = D_LineHeightFromFont(M->CodeFont, M->Buffer.HeightInPixels);


	f32 HeightOffset = 0;
	for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
	{
		D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(1600, WindowHeight - 500 + HeightOffset), v4f(1, 1, 1, 1),
			M->Buffer.HeightInPixels, Str8PushF(M->FrameArena, "Start: ?u32 | End: ?u32 | VisualX: ?u32",
			Cursor->Selection.X, Cursor->Selection.Y, Cursor->VisualX));
		HeightOffset -= M->Buffer.HeightInPixels;
	}

	int MainCursorLine = E_LineFromBufferIndex(M->Edit, M->Cursor.Selection.Y, &M->Buffer);
	for(int I = 0; I < 50; I++)
	{
		if(MainCursorLine == I)
		{
			D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, NumbersPosition,
				v4f(0.8, 0.3, 0, 1), LineHeight, Str8PushF(M->FrameArena, "?u32", I + 1));
		}
		else
		{
			D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, NumbersPosition,
					v4f(0.1, 0.04, 0, 1), LineHeight, Str8PushF(M->FrameArena, "?u32", I + 1));
		}
		NumbersPosition.Y -= LineHeight;
	}

	for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		D_DrawSelectionRange(M->Draw,
							M->FrameArena,
							M->CodeFont,
							BufferPosition,
							v4f(0.04, 0.01, 0, 0.1),
							M->Buffer.HeightInPixels,
							M->Buffer.String,
							Selection.X,
							Selection.Y);
	}
	
	D_DrawStr8(M->Draw,
			M->FrameArena, M->CodeFont,
			BufferPosition, v4f(0.8, 0.3, 0, 1),
			M->Buffer.HeightInPixels, M->Buffer.String);


	if(M->UI->MouseIsDragging || M->UI->MouseWasPressed)
	{
		M->Cursor.TargetPosition = E_GetPositionFromEditableBuffer(M->Edit,
																	M->Draw,
																	M->Cursor.Selection.Y,
																	M->Buffer,
																	M->CodeFont,
																	BufferPosition);
		M->Cursor.Position = M->Cursor.TargetPosition;
		D_DrawQuad(M->Draw, M->FrameArena, M->Cursor.Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{	
		for(cursor *Cursor = &M->Cursor; Cursor; Cursor = Cursor->Next)
		{
			Cursor->TargetPosition = E_GetPositionFromEditableBuffer(M->Edit,
																	M->Draw,
																	Cursor->Selection.Y,
																	M->Buffer,
																	M->CodeFont,
																	BufferPosition);

			Cursor->Position = LerpV2f(Cursor->Position, Cursor->TargetPosition, 0.3);
			D_DrawQuad(M->Draw, M->FrameArena, Cursor->Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
		}
	}

	if(M->UI->MouseIsDragging)
	{
		int Index = E_IndexFromPosition(M->Edit, M->Draw, M->CodeFont,
											&M->Buffer, BufferPosition, M->UI->MousePosition);
			
		M->Cursor.Selection.Y = Index;
	}
}

f32 Angle = 0.0f;


static u32 XORSHIFT_STATE = 2463534242;

u32 XorShift32() {
	XORSHIFT_STATE ^= XORSHIFT_STATE << 13;
	XORSHIFT_STATE ^= XORSHIFT_STATE >> 17;
	XORSHIFT_STATE ^= XORSHIFT_STATE << 5;
	return XORSHIFT_STATE;
}

float RandomBilateral() {
	float Result = (XorShift32() / (float)UINT32_MAX);

	return Result * 2.0f - 1.0f;
}

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);

	//D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
	
	m44f Transform = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	R_PushTransform(M->Render, Transform);
	//EditingWindow(M);

	Angle += 0.003;
	m44f ModelMatrix = M44FromYAngle(Angle);
	//ModelMatrix = MulM44FM44F(M44FromZAngle(Angle*0.1), ModelMatrix);
	m44f ProjectionMatrix = M44FPerspective(sapp_widthf()/sapp_heightf(), 30, 0.1, 100.0);
	m44f ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	m44f VP = MulM44FM44F(ViewMatrix, ProjectionMatrix);
	m44f PVM = MulM44FM44F(ModelMatrix, VP);
	R_PushTransform(M->Render, PVM);

	R_PushTransform(M->Render, VP);
	D_DrawGrid(M->Draw, M->FrameArena, 4.0, 25);
	R_PopTransform(M->Render);
	//D_DrawCubePoints(M->Draw, M->FrameArena, v3f(0, 0, 0.0), v3f(2.0, 2.0, 2.0), v4f(1, 0, 0, 1));
	
	// v3f Positions[3] =
	// {
	// 	v3f(0, 0, 0),
	// 	v3f(3, 0, 1),
	// 	v3f
	// };
	// D_DrawPointsArray(M->Draw, M->FrameArena, Positions, 2, v4f(1, 0, 0, 1));

	// v3f LinePoints[4] =
	// {
	// 	v3f(0, 0, 0),
	// 	v3f(0, 1, 0),
	// 	v3f(0, 0, 0),
	// 	v3f(3, 0, 1),
	// };
	// D_DrawLinesArray(M->Draw, M->FrameArena, LinePoints + 2, 2, v4f(1.0, 0.5, 0, 1));

	for(int I = 0; I < M->NodeCount; I++)
	{
		M->Nodes[I].Position = LerpV3f(M->Nodes[I].Position, M->Nodes[I].TargetPosition, 0.1);
	}
	
	R_PushTransform(M->Render, Transform);
	for(int I = 0; I < M->NodeCount; I++)
	{
		v3f P = M->Nodes[I].Position;
		v4f ProjectedPoint = MulM44FV4F(M44FTranspose(PVM), v4f(P.X, P.Y, P.Z, 1.0));
		ProjectedPoint.X /= ProjectedPoint.W;
		ProjectedPoint.Y /= ProjectedPoint.W;
		ProjectedPoint.Z /= ProjectedPoint.W;
		ProjectedPoint.W /= ProjectedPoint.W;

		ProjectedPoint.X = ProjectedPoint.X*0.5 + 0.5;
		ProjectedPoint.Y = ProjectedPoint.Y*0.5 + 0.5;

		v2f WindowSpacePoint = MulV2F(v2f(ProjectedPoint.X, ProjectedPoint.Y), v2f(sapp_widthf(), sapp_heightf()));

		D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, WindowSpacePoint, v4f(0.8, 0.3, 0.0, 1), 20, M->Nodes[I].Name);
	}
	R_PopTransform(M->Render);

	static f32 Temperature = 5.0;
	SimulateForceDirectedGraph(M->FrameArena, M->Nodes, M->NodeCount, M->Links, M->LinkCount, &Temperature);

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	v3f *Positions = (v3f*)ArenaPushZero(Scratch.Arena, sizeof(v3f)*M->NodeCount);
	for(int I = 0; I < M->NodeCount; I++)
	{
		Positions[I] = M->Nodes[I].Position;
	}

	v3f *LinePositions = (v3f*)ArenaPushZero(Scratch.Arena, sizeof(v3f)*M->LinkCount*2);
	int LineIndex = 0;
	for(int I = 0; I < M->LinkCount; I++)
	{
		LinePositions[LineIndex + 0] = M->Nodes[M->Links[I].From].Position;
		LinePositions[LineIndex + 1] = M->Nodes[M->Links[I].To].Position;
		LineIndex += 2;
	}

	D_DrawPointsArray(M->Draw, M->FrameArena, Positions, M->NodeCount, v4f(1, 0, 0, 1));
	D_DrawLinesArray(M->Draw, M->FrameArena, LinePositions, M->LinkCount*2, v4f(0, 1, 0, 1));

	R_PopTransform(M->Render);

	ScratchEnd(Scratch);
	
	//D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, WindowSpacePoint, v4f(0.8, 0.3, 0.0, 1), 50, Str8("Mayday/"));
	//D_DrawQuad(M->Draw, M->FrameArena, WindowSpacePoint, 10, 10, v4f(0, 0, 1, 1));
	if(OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenMousePressed) > 150.0 &&
	   M->UI->MouseWasPressed && M->UI->MultiClickCount == 0)
	{
		M->UI->MouseIsDragging = True;
	}
	else if(!M->UI->MouseWasPressed)
	{
		M->UI->MouseIsDragging = False;
	}
	

	if(0)
	{
		int LogoSize = 600;
		int LogoTextWidth = UI_GetTextWidth(M->LogoFont, M->Draw, Str8("MAYDAY!"), LogoSize);

		int Padding = 200;
		v4f Rect = v4f(Padding, Padding, WindowWidth - Padding*2, WindowHeight - Padding*2);
		D_DrawQuad(M->Draw, M->FrameArena, v2f(Rect.X, Rect.Y), Rect.Z, Rect.W, v4f(0.2, 0.06, 0, 1));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(Rect.X + 5, Rect.Y + 5), Rect.Z - 10, Rect.W - 10, v4f(0.009, 0.00, 0, 1.0));

		D_DrawQuad(M->Draw, M->FrameArena, v2f(WindowWidth/2.0 - LogoTextWidth/2.0, WindowHeight/2.0 - LogoSize/5.0), LogoTextWidth, LogoSize, v4f(0.009, 0.00, 0, 1.0));
		D_DrawStr8(M->Draw,
			M->FrameArena, M->LogoFont,
			v2f(WindowWidth/2.0 - LogoTextWidth/2.0, WindowHeight/2.0), v4f(0.4, 0.02, 0, 1), LogoSize, Str8("MAYDAY!"));
	}


	R_Update(M->Render, M->FrameArena);

	D_GlyphsToAtlas(M->Draw);

	ArenaReset(M->FrameArena);
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

	M.Draw = ArenaPushTypeZero(Arena, draw_context);
	M.Render = ArenaPushTypeZero(Arena, render_context);
	M.Edit = ArenaPushTypeZero(Arena, edit_context);
	M.UI = ArenaPushTypeZero(Arena, ui_context);
	M.Draw->Render = M.Render;
	M.Draw->Arena = Arena;
	M.Edit->Arena = Arena;
	M.Edit->Temp = ArenaPushZero(Arena, MegaBytes(1));

	M.Window = OS_MakeWindow(Str8("Mayday!"), 3024, 1964);

	M.CodeFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular);
	M.LogoFont = D_LoadFontFromTTF(M.Draw, G_BasementGrotesque);

	M.UI->Font = M.CodeFont;


	os_directory_tree Tree = OS_DirectoryTreeFromPath(M.PermanentArena, Str8("/Users/olivercruickshank/Mayday"));

	os_directory_entry *Entry = Tree.Root;

	int TotalCount = OS_CountAllEntriesInDirectoryTree(Tree);
	LogInfo("Total ?s32", TotalCount);

	M.Nodes = (fdg_node*)ArenaPushZero(M.PermanentArena, sizeof(fdg_node)*TotalCount);
	M.Links = (fdg_link*)ArenaPushZero(M.PermanentArena, sizeof(fdg_link)*TotalCount);

	os_directory_tree_iterator Iter = {.Tree = Tree, .Current = Tree.Root };

	M.NodeCount = TotalCount;
	M.Nodes[0].TargetPosition = v3f(0, 0, 0);
	M.Nodes[0].Position = v3f(0.0);
	M.Nodes[0].Name = Tree.Root->Name;
	int Index = 1;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{
		M.Nodes[Index].TargetPosition = v3f(RandomBilateral()*0.5, RandomBilateral()*0.5, RandomBilateral()*0.5);
		M.Nodes[Index].Position = M.Nodes[Index].TargetPosition;
		M.Nodes[Index].Name = Iter.Current->Name;
		//LogInfo("?S", Iter.Current->Name);
	}

	Iter.Current = Tree.Root;
	int From = 0;
	int To = 1;
	Index = 0;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{
		//if(Index >= 5) break;
		M.Links[Index].From = From;
		M.Links[Index].To = To;
		if(Iter.Current->Type == OS_DIR_ENTRY_FOLDER)
		{
			From = To;
		}
		To++;
		Index++;
	}
	// M.NodeCount = 5;
	M.LinkCount = Index;

	OS_Run(M.Window, &M);
}