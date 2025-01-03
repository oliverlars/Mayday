
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


	v3f LookAt;
	v3f LookFrom;
	v3f LookAtTarget;
	v3f LookFromTarget;
	f32 ViewSize;
	f32 ViewSizeTarget;

	random32_state RandomState;

	os_directory_tree Tree;

	b32 ShowEditor;
}
mayday_context;

#include "shaders.h"

void TreeData(mayday_context *M)
{
	os_directory_entry *Entry = M->Tree.Root;

	int TotalCount = OS_CountAllEntriesInDirectoryTree(M->Tree);
	LogInfo("Total ?s32", TotalCount);

	M->Nodes = (fdg_node*)ArenaPushArrayZero(M->PermanentArena, fdg_node, TotalCount);
	M->Links = (fdg_link*)ArenaPushArrayZero(M->PermanentArena, fdg_link, TotalCount);

	os_directory_tree_iterator Iter = {.Tree = M->Tree, .Current = M->Tree.Root };

	M->NodeCount = TotalCount;
	M->Nodes[0].TargetPosition = v3f(0, 0, 0);
	M->Nodes[0].Position = v3f(0.0);
	M->Nodes[0].Name = M->Tree.Root->PathAndName;
	M->Nodes[0].IsFolder = True;
	int Index = 1;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{
		M->Nodes[Index].TargetPosition = v3f(RandomBilateral(&M->RandomState),
											RandomBilateral(&M->RandomState),
											RandomBilateral(&M->RandomState));
		M->Nodes[Index].TargetPosition = NormalizeV3F(M->Nodes[Index].TargetPosition);
		M->Nodes[Index].Position = M->Nodes[Index].TargetPosition;
		M->Nodes[Index].Name = Iter.Current->PathAndName;
		M->Nodes[Index].IsFolder = Iter.Current->Type == OS_DIR_ENTRY_FOLDER;
		Index++;
	}

	Iter.Current = M->Tree.Root;
	int From = 0;
	int To = 1;
	Index = 0;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{
		M->Links[Index].From = From;
		M->Links[Index].To = To;
		if(Iter.Current->Type == OS_DIR_ENTRY_FOLDER)
		{
			From = To;
		}
		To++;
		Index++;
	}
	M->LinkCount = Index;
}


f32 Angle = 0.0f;


b32 ZoomedIn = False;

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	M->UI->Hot = 0;

	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);

	if(!M->Nodes)
	{
		TreeData(M);
	}

	m44f Transform = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	R_PushTransform(M->Render, Transform);

	D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
	if(M->ShowEditor)
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

	Angle += M->UI->ScrollOffset.X/1000.0;


	Angle = LerpF32(Angle, 0.0, 0.05);

	m44f ModelMatrix = M44F_IDENTITY;

	if(ZoomedIn)
	{
		ModelMatrix = M44FTranslate(MulV3F(M->LookAt, V3FromF32(1.0)));
		ModelMatrix = MulM44FM44F(M44FromYAngle(-Angle), ModelMatrix);
		ModelMatrix = MulM44FM44F(M44FTranslate(MulV3F(M->LookAt, V3FromF32(-1.0))), ModelMatrix);
	}
	else
	{
		ModelMatrix = M44FromYAngle(-Angle);
	}

	//ModelMatrix = MulM44FM44F(M44FromZAngle(Angle*0.1), ModelMatrix);
	m44f ProjectionMatrix = M44FPerspective(sapp_widthf()/sapp_heightf(), 70, 0.01, 1000.0);
	ProjectionMatrix = M44FOrthographic(-M->ViewSize, M->ViewSize, M->ViewSize, -M->ViewSize, -1000.0, 1000.0);
	m44f ViewMatrix = M44FLookAt(M->LookFrom, M->LookAt, v3f(0, 1, 0));

	m44f VP = MulM44FM44F(ViewMatrix, ProjectionMatrix);
	m44f PVM = MulM44FM44F(ModelMatrix, VP);
	R_PushTransform(M->Render, PVM);

	R_PushTransform(M->Render, VP);
	if(!M->ShowEditor)
	{
		//D_DrawGrid(M->Draw, M->FrameArena, 0.5, 20);
	}
	R_PopTransform(M->Render);

	for(int I = 0; I < M->NodeCount; I++)
	{
		M->Nodes[I].Position = LerpV3f(M->Nodes[I].Position, M->Nodes[I].TargetPosition, 0.1);
	}

	R_PushTransform(M->Render, Transform);

	fdg_node *ClickedNode = Null;
	for(int I = 0; !M->ShowEditor && I < M->NodeCount; I++)
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

		WindowSpacePoint = AddV2F(WindowSpacePoint, v2f(20, 10));


		f32 FontSize = ZoomedIn ? 40 : 35;
		if(M->Nodes[I].IsFolder)
		{
			
			b32 Clicked = UI_TextButton(M->UI, M->Draw,
				  M->FrameArena, Str8PushF(M->FrameArena, "?S##?u32", Str8FromLastCharacter(M->FrameArena, M->Nodes[I].Name, '/'), I),
				  FontSize,
				  WindowSpacePoint, v4f(0.5, 0.1, 0.0, 1));
			
			if(Clicked && !ZoomedIn)
			{
				M->UI->MouseWasPressed = False;
				ClickedNode = M->Nodes + I;

				M->Tree = OS_DirectoryTreeFromPath(M->PermanentArena, Str8PushF(M->PermanentArena, "?S", ClickedNode->Name));
				TreeData(M);
				M->LookFromTarget = v3f(0.0);
				M->LookAtTarget = v3f(0.0);
				M->ViewSizeTarget = M->ViewSize;
			}
			//D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, WindowSpacePoint, v4f(0.5, 0.1, 0.0, 1), 25, M->Nodes[I].Name);
		}
		
		if(ZoomedIn && !M->Nodes[I].IsFolder && !ClickedNode)
		{
			b32 Clicked = UI_TextButton(M->UI, M->Draw,
				  M->FrameArena, Str8PushF(M->FrameArena, "?S##?u32", Str8FromLastCharacter(M->FrameArena, M->Nodes[I].Name, '/'), I),
				  FontSize,
				  WindowSpacePoint, v4f(0.5, 0.1, 0.0, 1));
			if(Clicked && !M->ShowEditor)
			{
				m_scratch_arena Scratch = ScratchBegin(0, 0);
				str8 Temp = OS_LoadEntireFileIntoMemory(Scratch.Arena, M->Nodes[I].Name);
				MemoryCopy(M->Buffer.String.Bytes, Temp.Bytes, Temp.Size);
				M->Buffer.String.Size = Temp.Size;
				M->ShowEditor = True;
				ScratchEnd(Scratch);
				ZoomedIn = False;
				M->UI->MouseWasPressed = False;
			}
		}
	}

	// if(ClickedNode)
	// {
	// 	M->ViewSizeTarget = 1.0;
	// 	M->LookAtTarget = ClickedNode->TargetPosition;
	// 	M->LookFromTarget = ClickedNode->TargetPosition;
	// 	M->LookFromTarget.Z += 0.5;
	// 	ZoomedIn = True;
	// }

	M->LookAt = LerpV3f(M->LookAt, M->LookAtTarget, 0.1);
	M->LookFrom = LerpV3f(M->LookFrom, M->LookFromTarget, 0.1);
	M->ViewSize = LerpF32(M->ViewSize, M->ViewSizeTarget, 0.1);

	if(!M->ShowEditor)
	{
		D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
	}

	R_PopTransform(M->Render);

	static f32 Temperature = 10.0;
	octree_header Octree = {0};
	Octree.Bounds = ArenaPushArrayZero(M->FrameArena, bounds, M->NodeCount*4);
	Octree.ChildrenCounts = ArenaPushArrayZero(M->FrameArena, int, M->NodeCount*4);
	Octree.Nodes = ArenaPushArrayZero(M->FrameArena, octant_node*, M->NodeCount*4);
	Octree.Masses = ArenaPushArrayZero(M->FrameArena, v3f, M->NodeCount*4);
	Octree.Octants = ArenaPushArrayZero(M->FrameArena, octant, M->NodeCount*4);

	Octree.Bounds[0] = GetBounds(M->Nodes, M->NodeCount);
	Octree.NextOctantIndex = 1;

	for(int I = 0; I < M->NodeCount; I++)
	{
		OctreeBuild(M->FrameArena, &Octree, 0, &M->Nodes[I], 0);
	}
	
	Octree.Masses[0] = OctreeCalculateAttributes(&Octree, 0);

	SimulateForceDirectedGraphOctree(M->FrameArena, M->Nodes, M->NodeCount, M->Links, M->LinkCount, &Temperature, &Octree);

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	v3f *Positions = (v3f*)ArenaPushArrayZero(Scratch.Arena, v3f, M->NodeCount);
	for(int I = 0; I < M->NodeCount; I++)
	{
		Positions[I] = M->Nodes[I].Position;
	}

	v3f *LinePositions = ArenaPushArrayZero(Scratch.Arena, v3f, M->LinkCount*2);
	int LineIndex = 0;
	for(int I = 0; I < M->LinkCount; I++)
	{
		LinePositions[LineIndex + 0] = M->Nodes[M->Links[I].From].Position;
		LinePositions[LineIndex + 1] = M->Nodes[M->Links[I].To].Position;
		LineIndex += 2;
	}

	if(!M->ShowEditor)
	{
		D_DrawPointsArray(M->Draw, M->FrameArena, Positions, M->NodeCount, v4f(1, 0, 0, 1));
		D_DrawLinesArray(M->Draw, M->FrameArena, LinePositions, M->LinkCount*2, v4f(0.5, 0.1, 0, 1));

		bounds TotalBounds = GetBounds(M->Nodes, M->NodeCount);

		//DrawBounds(M->Draw, M->FrameArena, Octree.Bounds[0]);

		v3f Centre = DivV3F(AddV3F(TotalBounds.Min, TotalBounds.Max), V3FromF32(2.0));


		if(!ZoomedIn)
		{
			M->LookAtTarget = Centre;
			M->LookFromTarget = v3f(Centre.X, Centre.Y, 5);
		}
		DrawOctree(M->Draw, M->FrameArena, &Octree, 0);
	}

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

	M.Tree = OS_DirectoryTreeFromPath(M.PermanentArena, Str8PushF(Arena, "/Users/olivercruickshank/EmberGen"));

	M.RandomState = XorShiftInit();

	bounds TotalBounds = GetBounds(M.Nodes, M.NodeCount);
	M.LookFrom = v3f(0.0);
	M.LookFromTarget = v3f(0.0);
	M.LookAt = v3f(0.0);
	M.LookAtTarget = v3f(0.0);
	M.ViewSize = 5.0;
	M.ViewSizeTarget = M.ViewSize;

	OS_Run(M.Window, &M);
}