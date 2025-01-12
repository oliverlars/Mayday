#include "draw.h"
#include "edit.h"
#include "memory.h"
#include "ui.h"

void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   editable_buffer *Buffer,
			   cursor *Cursors,
			   v2i Size)
{
	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = Size.Y;
	int WindowWidth = Size.X;

	v2f BufferPosition = v2f(100, (f32)WindowHeight - 50 + UI->ScrollOffset.Y*15);

	v2f NumbersPosition = v2f(10, (f32)WindowHeight - 50 + UI->ScrollOffset.Y*15);

	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);


	f32 HeightOffset = 0;
	for(cursor *Cursor = Cursors; Cursor; Cursor = Cursor->Next)
	{
		// D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(1600, WindowHeight - 500 + HeightOffset), v4f(1, 1, 1, 1),
		// 	M->Buffer.HeightInPixels, Str8PushF(M->FrameArena, "Start: ?u32 | End: ?u32 | VisualX: ?u32",
		// 	Cursor->Selection.X, Cursor->Selection.Y, Cursor->VisualX));
		HeightOffset -= Buffer->HeightInPixels;
	}

	int MainCursorLine = E_LineFromBufferIndex(Edit, Cursors[0].Selection.Y, Buffer);
	int LineCount = 0;
	for(int I = 0; I < Buffer->String.Size; I++)
	{
		if(MainCursorLine == LineCount)
		{
			D_DrawStr8(Draw, Arena, Font, NumbersPosition,
				v4f(0.8, 0.3, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
		}
		else
		{
			D_DrawStr8(Draw, Arena, Font, NumbersPosition,
					   v4f(0.1, 0.04, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
		}

		if(Buffer->String.Bytes[I] == '\n')
		{
			LineCount += 1;
			NumbersPosition.Y -= LineHeight;
		}
	}

	for(cursor *Cursor = Cursors; Cursor; Cursor = Cursor->Next)
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


	if(UI->MouseIsDragging || UI->MouseWentDown)
	{
		Cursors[0].TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	Draw,
																	Cursors[0].Selection.Y,
																	Buffer[0],
																	Font,
																	BufferPosition);
		Cursors[0].Position = Cursors[0].TargetPosition;
		D_DrawQuad(Draw, Arena, Cursors[0].Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{
		for(cursor *Cursor = Cursors; Cursor; Cursor = Cursor->Next)
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
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition, UI->MousePosition);

		Cursors[0].Selection.Y = Index;
	}


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
