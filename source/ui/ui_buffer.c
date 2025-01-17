#include "draw.h"
#include "edit.h"
#include "memory.h"
#include "os.h"
#include "ui.h"

#include <assert.h>

void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   editable_buffer *Buffer,
			   v2i Size)
{
	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = Size.Y;
	int WindowWidth = Size.X;

	v2f BufferPosition = v2f(100, (f32)WindowHeight - 50);
	BufferPosition.Y += Buffer->Offset.Y;

	v2f NumbersPosition = v2f(10, BufferPosition.Y);

	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);


	f32 HeightOffset = 0;
	for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
	{
		// D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(1600, WindowHeight - 500 + HeightOffset), v4f(1, 1, 1, 1),
		// 	M->Buffer.HeightInPixels, Str8PushF(M->FrameArena, "Start: ?u32 | End: ?u32 | VisualX: ?u32",
		// 	Cursor->Selection.X, Cursor->Selection.Y, Cursor->VisualX));
		HeightOffset -= Buffer->HeightInPixels;
	}

	int MainCursorLine = E_LineFromBufferIndex(Edit, Edit->Cursors[0].Selection.Y, Buffer);
	int LineCount = 0;
	for(int I = 0; I < Buffer->String.Size; I++)
	{
		if(MainCursorLine == LineCount)
		{
			D_DrawStr8(Draw, Arena, Font, NumbersPosition,
				v4f(0.8, 0.3, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
		}

		if(Buffer->String.Bytes[I] == '\n')
		{
			if(MainCursorLine != LineCount)
			{
				D_DrawStr8(Draw, Arena, Font, NumbersPosition,
						   v4f(0.1, 0.04, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
			}
			LineCount += 1;
			NumbersPosition.Y -= LineHeight;
		}
	}

	for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		D_DrawSelectionRange(Draw,
							 Arena,
							 Font,
							 BufferPosition,
							 v4f(0.04, 0.01, 0, 0.1),
							 Buffer->HeightInPixels,
							 Buffer->String,
							 Selection.X,
							 Selection.Y);
	}

	D_DrawStr8(Draw,
			   Arena, Font,
			   BufferPosition, v4f(0.8, 0.3, 0, 1),
			   Buffer->HeightInPixels, Buffer->String);

	//if(UI->MouseIsDragging || UI->MouseWentDown)
	if(UI->MouseIsDragging)
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition, UI->MousePosition);
		Edit->Cursors[0].Selection.Y = Index;

		Edit->Cursors[0].TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	Draw,
																	Edit->Cursors[0].Selection.Y,
																	Buffer[0],
																	Font,
																	BufferPosition);
		Edit->Cursors[0].Position = Edit->Cursors[0].TargetPosition;
		D_DrawQuad(Draw, Arena, Edit->Cursors[0].Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{
		for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
		{
			Cursor->TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	 Draw,
																	 Cursor->Selection.Y,
																	 Buffer[0],
																	 Font,
																	 BufferPosition);

			Cursor->Position = LerpV2F(Cursor->Position, Cursor->TargetPosition, 0.3);
			D_DrawQuad(Draw, Arena, Cursor->Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
		}
	}

	if(UI->MouseIsDragging)
	{
	}

	v2f ScrollOffset = UI_Scroll(UI);
	Buffer->Offset.X += ScrollOffset.X*15;
	Buffer->Offset.Y = Max(Buffer->Offset.Y + ScrollOffset.Y*15, 0.0);


	if(UI_DoubleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;

		v2i InitialSelection = Cursor->Selection;
		E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
		v2i Selection = Cursor->Selection;
		Cursor->Selection = InitialSelection;
		E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
		Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_TripleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;
		if(False)
		{
			cursor NextCursor = Cursor[0];
			NextCursor.Selection = v2i(Index, Index);
			E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
			Cursor = Cursor->Next;
		}
		else
		{
			Edit->Cursors[0].Next = Null;
		}

		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineEnd = E_LineEndFromIndex(Edit, Cursor->Selection.Y, Buffer);
		Cursor->Selection = v2i(LineStart, LineEnd);

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_QuadClicked(UI))
	{
		Edit->Cursors[0].Selection = v2i(0, Buffer->String.Size);

		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_LeftMouseWentDown(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors->Next = Null;
		Cursor = Edit->Cursors;
		Cursor->Selection = v2i(Index, Index);

	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_DOWN, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_DOWN, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_DOWN, SAPP_MODIFIER_SUPER))
	{
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor NextCursor = LastCursor[0];
		E_MoveCursors(Edit, Draw, Font, &NextCursor, Buffer, E_MOVE_DOWN, E_MOVE_NOT_SELECTING);
		E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_UP, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_UP, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_UP, SAPP_MODIFIER_SUPER))
	{
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor NextCursor = LastCursor[0];
		E_MoveCursors(Edit, Draw, Font, &NextCursor, Buffer, E_MOVE_UP, E_MOVE_NOT_SELECTING);
		E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_LEFT, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, SAPP_MODIFIER_SHIFT))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_LEFT, E_MOVE_SELECTING);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_RIGHT, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, SAPP_MODIFIER_SHIFT))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_RIGHT, E_MOVE_SELECTING);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_BACKSPACE, 0))
	{
		E_DeleteTo(Edit, Edit->Cursors, Buffer);
	}

	str8 TextToInsert = UI_Text(UI, Arena);
	if(TextToInsert.Size > 0)
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, TextToInsert);
	}

	// else if(UI_LeftMouseWentDownWithModifiers(UI, SAPP_MODIFIER_ALT))
	// {
	// 	int Index = E_IndexFromPosition(Edit, Draw, Font,
	// 								Buffer, BufferPosition,
	// 								UI->MousePosition);
	// 	cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
	// 	cursor *Cursor = LastCursor;
	// 	{
	// 		cursor NextCursor = Cursor[0];
	// 		NextCursor.Selection = v2i(Index, Index);
	// 		E_AppendCursor(Edit, Edit->Arena, Cursor, NextCursor);
	// 		Cursor = Cursor->Next;
	// 	}
	// }
}

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer,
				  v2i Size)
{
	D_DrawQuad(Draw, Arena, v2f(0, 0), (f32)Size.X, 50, v4f(0.3, 0.08, 0, 1));

	D_DrawStr8(Draw, Arena, Font, v2f((f32)Size.X - 200, 10), v4f(0.009, 0.00, 0, 1.0), 50, Str8("LF"));

	cursor *Cursor = Cursors;
	if(!Cursor->Next)
	{
		int Line = E_LineFromBufferIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int Column = Cursor->Selection.Y - LineStart;
		D_DrawStr8(Draw, Arena,
				   Font, v2f((f32)Size.X - 1000, 10),
				   v4f(0.009, 0.00, 0, 1.0),
				   50, Str8PushF(Arena, "?u32:?u32", Line, Column));
	}
	else
	{
		int Count = 0;
		for(;Cursor;Cursor = Cursor->Next)
		{
			Count += 1;
		}
		D_DrawStr8(Draw, Arena, Font, v2f((f32)Size.X - 1000, 10),
				   v4f(0.009, 0.00, 0, 1.0), 35,
				   Str8PushF(Arena, "?u32 SELECTIONS", Count));
	}
}
