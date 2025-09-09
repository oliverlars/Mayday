#include "draw.h"
#include "edit.h"
#include "maths.h"
#include "memory.h"
#include "os.h"
#include "render.h"
#include "strings.h"
#include "ui.h"

#include <assert.h>

#include <math.h>


// HACK(Oliver): Remove the ifdef...
#ifdef __APPLE
#include <arm_neon.h>
s64 CountNewLinesNEON(str8 String)
{
	u64 Count = 0;
	s64 Index = 0;

	for (; Index <= String.Size - 16; Index += 16)
	{
		uint8x16_t Lane = vld1q_u8((const uint8_t *)(String.Bytes + Index));
		uint8x16_t Mask = vceqq_u8(Lane, vdupq_n_u8('\n'));
		uint8x16_t NewLinesThisLane = vandq_u8(Mask, vdupq_n_u8(1));
		Count += vaddvq_u8(NewLinesThisLane);
	}

	for (; Index < String.Size; Index++)
	{
		if (String.Bytes[Index] == '\n')
		{
			Count++;
		}
	}

	return Count;
}
#else
s64 CountNewLinesNEON(str8 String)
{
	u64 Count = 0;
	s64 Index = 0;

	for (; Index <= String.Size; Index += 1)
	{
		if(String.Bytes[Index] == '\n') Count++;
	}

	return Count;
}
#endif

s64 IndexFromLine(str8 String, int Line)
{
	u64 Count = 0;
	s64 Index = 0;

	for (; Index < String.Size ; Index++)
	{
		if (String.Bytes[Index] == '\n')
		{
			if(Count == Line) break;
			Count++;
		}
	}

	return Index;

	// TODO(Oliver):
	// Make this use SIMD
}

int DigitCountFromInt(u32 Value) {
	int Result = 0;
	if (Value == 0) return 1;
	for(;Value != 0; Value /= 10)
	{
		Result += 1;
	}
	return Result;
}

void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   str8 BufferPath, editable_buffer *Buffer,
			   v2i Size)
{
	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = Size.Y;
	int WindowWidth = Size.X;

	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);

	m44f Transform = R_CurrentTransform(Draw->Render);
	Transform = Mul(Translate(v3f(Buffer->Offset.X, fmod(Buffer->Offset.Y, LineHeight), 0)), Transform);
	R_PushTransform(Draw->Render, Transform);

	int NumberOfLines = CountNewLinesNEON(Buffer->String);

	f32 Margin = 10.0f;
	f32 MinOffsetX = 4 * UI_GetTextWidth(Font, Draw, Str8("0"), Buffer->HeightInPixels);

	f32 NumberOffsetX = Max(MinOffsetX, DigitCountFromInt(NumberOfLines) * UI_GetTextWidth(Font, Draw, Str8("0"), Buffer->HeightInPixels));

	v2f BufferPosition = v2f(NumberOffsetX, (f32)WindowHeight);

	v2f NumbersPosition = v2f(Margin, BufferPosition.Y);

	int FirstVisibleLine = Max((Buffer->Offset.Y)/LineHeight, 0);
	int LastVisibleLine = FirstVisibleLine + WindowHeight/LineHeight;

	int StartIndex = IndexFromLine(Buffer->String, FirstVisibleLine);
	int EndIndex = IndexFromLine(Buffer->String, LastVisibleLine);

	str8 SubString = Buffer->String;
	SubString.Bytes += StartIndex;
	SubString.Size = EndIndex - StartIndex;
	v2i Range = {StartIndex, EndIndex};

	// D_Str8(Draw, Arena, Font, v2f(800, 400),
	// 		   v4f(1.0, 1.0, 1, 1), LineHeight, Str8PushF(Arena, "FirstVsibleLine: ?u32 LastVisibleLine: ?u32", FirstVisibleLine, LastVisibleLine));

	// D_Str8(Draw, Arena, Font, v2f(900, 300),
	// 		   v4f(1.0, 1.0, 1, 1), LineHeight, Str8PushF(Arena, "StartIndex: ?u32 EndIndex: ?u32", StartIndex, EndIndex));

	int MainCursorLine = E_LineFromBufferIndex(Edit, Edit->Cursors[0].Selection.Y, Buffer);
	for(int I = FirstVisibleLine; I <= LastVisibleLine; I++)
	{
		if(MainCursorLine == I)
		{
			D_Str8(Draw, Arena, Font, NumbersPosition,
				v4f(0.8, 0.3, 0, 1), LineHeight, Str8PushF(Arena, "?u32", I + 1));
		}
		else
		{
			D_Str8(Draw, Arena, Font, NumbersPosition,
					   v4f(0.1, 0.05, 0, 1), LineHeight, Str8PushF(Arena, "?u32", I + 1));
		}
		NumbersPosition.Y -= LineHeight;
	}

	for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		str8 SelectedString = Buffer->String;
		D_SelectionRange(Draw,
							 Arena,
							 Font,
							 BufferPosition,
							 v4f(0.04, 0.01, 0, 0.8),
							 Buffer->HeightInPixels,
							 Buffer->String,
							 Range,
							 Selection.X,
							 Selection.Y);

		f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
		int CursorStartLine = E_LineFromBufferIndex(Edit, StartIndex, Buffer);
		int CursorEndLine = E_LineFromBufferIndex(Edit, EndIndex, Buffer);
		// HACK(Oliver): The start position should be for the buffer, 0 isn't necessarily always the start...
		// Also the colours are just random guesses, need a better way of formalising these.
		D_Quad(Draw, Arena, v2f(0, Cursor->TargetPosition.Y),
					sapp_widthf(), (Font.Ascent - Font.Descent + Font.LineGap)*Scale, v4f(0.04, 0.01, 0, 0.25));
	}



	D_Str8(Draw,
			   Arena, Font,
			   BufferPosition, v4f(0.8, 0.3, 0, 1),
			   Buffer->HeightInPixels, SubString);

	v2f ScrollOffset = UI_Scroll(UI);

	if(fabs(ScrollOffset.X) > fabs(ScrollOffset.Y))
	{
		Buffer->Offset.X = Min(Buffer->Offset.X + ScrollOffset.X*15, 0.0);
	}
	else
	{
		Buffer->Offset.Y = Max(Buffer->Offset.Y + ScrollOffset.Y*15, 0.0);
	}

	if(UI->MouseIsDragging)
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, Range,
										BufferPosition, UI->MousePosition);
		Edit->Cursors[0].Selection.Y = Index;

		Edit->Cursors[0].TargetPosition = E_GetVisualPositionFromEditableBuffer(Edit,
																	Draw,
																	Edit->Cursors[0].Selection.Y,
																	Buffer[0],
																	Font,
																	Range,
																	BufferPosition);
		Edit->Cursors[0].Position = Edit->Cursors[0].TargetPosition;
		D_Quad(Draw, Arena, Edit->Cursors[0].Position, Min(LineHeight/10.0, 2), LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{
		for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
		{
			if(Cursor->Selection.Y > Range.X && Cursor->Selection.Y < Range.Y)
			{
				Cursor->TargetPosition = E_GetVisualPositionFromEditableBuffer(Edit,
																		 Draw,
																		 Cursor->Selection.Y,
																		 Buffer[0],
																		 Font,
																		 Range,
																		 BufferPosition);
			}
			if(fabs(ScrollOffset.X) > 0.0f || fabs(ScrollOffset.Y) > 0.0f)
			{
				Cursor->Position = Cursor->TargetPosition;
			}
			else
			{
				Cursor->Position = LerpV2F(Cursor->Position, Cursor->TargetPosition, 0.3);
			}
			int CursorLine = E_LineFromBufferIndex(Edit, Cursor->Selection.Y, Buffer);
			if(FirstVisibleLine < CursorLine && CursorLine < LastVisibleLine)
			{
				D_Quad(Draw, Arena, Cursor->Position, Min(LineHeight/10.0, 2), LineHeight, v4f(0.3, 0.7, 0, 1));
			}

		}
	}

	if(UI_DoubleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit->Cursors);
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
										Buffer, Range, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors[0].Next = Null;

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
										Buffer, Range, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit->Cursors);
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
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors->Next = Null;
		Cursor = Edit->Cursors;
		Cursor->Selection = v2i(Index, Index);

	}
	
	if(UI_KeyDown(UI, SAPP_KEYCODE_ESCAPE, 0))
	{
		cursor *LastCursor = E_LastCursor(Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors->Next = Null;
		Cursor = Edit->Cursors;
		Cursor->Selection.X = Cursor->Selection.Y;
	}


	if(UI_KeyDown(UI, SAPP_KEYCODE_ENTER, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8("\n"));

		int TotalWhiteSpaceInserted = 0;
		for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
		{
			int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);

			int FirstNonWhitespace = LineStart;
			for(;Buffer->String.Bytes[FirstNonWhitespace] == ' ' ||
				 Buffer->String.Bytes[FirstNonWhitespace] == '\t';)
			{
				FirstNonWhitespace++;
			}
			str8 WhiteSpaceToInsert = (str8){	FirstNonWhitespace - LineStart, Buffer->String.Bytes + LineStart};

			E_InsertTextAtIndex(Edit, Cursor->Selection.Y, Buffer, WhiteSpaceToInsert);
			TotalWhiteSpaceInserted += WhiteSpaceToInsert.Size;
			Cursor->Selection.Y += TotalWhiteSpaceInserted;
			Cursor->Selection.X = Cursor->Selection.Y;
		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_A, SAPP_MODIFIER_SUPER))
	{
		cursor *LastCursor = E_LastCursor(Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors->Next = Null;
		Edit->Cursors = Cursor;
		Cursor->Selection.X = 0;
		Cursor->Selection.Y =Buffer->String.Size;

	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_D, SAPP_MODIFIER_SUPER))
	{
		str8 StringToFind = {};
		cursor *Cursor = E_LastCursor(Edit->Cursors);
		if(Cursor->Selection.X == Cursor->Selection.Y)
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, Range, BufferPosition,
										UI->MousePosition);
			v2i InitialSelection = Cursor->Selection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
			v2i Selection = Cursor->Selection;
			Cursor->Selection = InitialSelection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
			Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);
		}
		else
		{
			v2i Sorted = E_GetSortedSelection(Cursor->Selection);
			StringToFind.Size = Sorted.Y - Sorted.X;
			StringToFind.Bytes = Buffer->String.Bytes + Sorted.X;

			// TODO(Oliver): Unicode
			u8 *Start = Buffer->String.Bytes;
			u8 *End = Buffer->String.Bytes + Buffer->String.Size;
			for(u8 *Byte = Buffer->String.Bytes + Sorted.Y + 1; Byte < End; Byte++)
			{
				str8 CurrentString = {0};
				CurrentString.Bytes = Byte;
				CurrentString.Size = Min(StringToFind.Size, End-Byte);
				if(Str8Equal(CurrentString, StringToFind))
				{
					cursor NextCursor = Cursor[0];
					NextCursor.Selection.X = Byte-Start;
					NextCursor.Selection.Y = NextCursor.Selection.X + StringToFind.Size;
					E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
					break;
				}
			}

		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_TAB, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8("\t"));
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_P, SAPP_MODIFIER_SUPER))
	{
		UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
		UI->View = UI_VIEW_TREE;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_S, SAPP_MODIFIER_SUPER))
	{
		if(OS_GetFileLastWriteTimestamp(BufferPath) != Buffer->LastModifiedTime)
		{
			LogError("NO ALLOWED");
		}
		else
		{
			LogInfo("THIS IS FINE");
			OS_WriteStringToDisk(Arena, BufferPath, Buffer->String);
			Buffer->LastModifiedTime = OS_GetFileLastWriteTimestamp(BufferPath);
		}
	}

	b32 ShouldRemove = False;
	for(ui_event_iterator It = {0};UI_EventIterate(UI, &It);)
	{
		// HACK(Oliver): Why the fuck does this happen anyway????
		// It should not be null, even if we remove. it should just handle
		// that correctly
		if(!It.Current) break;
		sapp_event Event = It.Current->E;
		if(ShouldRemove)
		{
			assert(It.Current && It.Current->Previous);
			UI_RemoveEventFromList(UI, It.Current->Previous);
			ShouldRemove = False;
		}
		if(Event.type == SAPP_EVENTTYPE_KEY_DOWN)
		{
			edit_move_type MoveType = E_MOVE_NOT_SELECTING;
			if(Event.modifiers & SAPP_MODIFIER_SHIFT)
			{
				MoveType = E_MOVE_SELECTING;
			}

			edit_movement Movement = E_MOVE_NONE;
			switch(Event.key_code)
			{
				case SAPP_KEYCODE_LEFT:
				{
					Movement = E_MOVE_LEFT;
					if(Event.modifiers & SAPP_MODIFIER_ALT)
					{
						Movement = E_MOVE_WORD_LEFT;
					}
					else if (Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						Movement = E_MOVE_LINE_START;
					}

					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, Movement, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_RIGHT:
				{
					Movement = E_MOVE_RIGHT;
					if(Event.modifiers & SAPP_MODIFIER_ALT)
					{
						Movement = E_MOVE_WORD_RIGHT;
					}
					else if (Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						Movement = E_MOVE_LINE_END;
					}

					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, Movement, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_UP:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER && Event.modifiers & SAPP_MODIFIER_ALT)
					{
						cursor *LastCursor = E_LastCursor(Edit->Cursors);
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
						E_MoveCursors(Edit, Draw, Font, E_LastCursor(Edit->Cursors), Buffer, E_MOVE_UP, MoveType);
					}
					else
					{
						E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_UP, MoveType);
					}
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_DOWN:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER && Event.modifiers & SAPP_MODIFIER_ALT)
					{
						cursor *LastCursor = E_LastCursor(Edit->Cursors);
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
						E_MoveCursors(Edit, Draw, Font, E_LastCursor(Edit->Cursors), Buffer, E_MOVE_DOWN, MoveType);
					}
					else
					{
						E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_DOWN, MoveType);
					}
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_BACKSPACE:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						cursor *Cursor = E_LastCursor(Edit->Cursors);
						E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_SELECTING);
					}
					E_DeleteTo(Edit, Edit->Cursors, Buffer);
					
				}
				break;
				default:
				break;
			}
		}
	}

	str8 TextToInsert = UI_Text(UI, Arena);
	if(TextToInsert.Size > 0)
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, TextToInsert);
	}

	if(UI_LeftMouseWentDownWithModifiers(UI, SAPP_MODIFIER_ALT))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit->Cursors);
		cursor *Cursor = LastCursor;
		{
			cursor NextCursor = Cursor[0];
			NextCursor.Selection = v2i(Index, Index);
			E_AppendCursor(Edit, Edit->Arena, Cursor, NextCursor);
			Cursor = Cursor->Next;
		}
	}

	R_PopTransform(Draw->Render);
}

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer, v2i Size)
{

	int Ascent, Descent, LineGap;
	stbtt_GetFontVMetrics(&Font.Info, &Ascent, &Descent, &LineGap);

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels * 1.2);
	float AscentInPixels = Ascent  * Scale;
	float DescentInPixels = Descent * Scale;

	float TextHeight = (AscentInPixels - DescentInPixels);

	f32 StatusBarHeight = Buffer->HeightInPixels * 1.2f;

	float CentreOffset = (AscentInPixels + DescentInPixels) * 0.5f;

	f32 TextPosY  = (StatusBarHeight * 0.5f) - CentreOffset;

	D_Quad(Draw, Arena, v2f(0, 0), (f32)Size.X, StatusBarHeight, v4f(0.3, 0.08, 0, 1));

	D_Str8(Draw, Arena, Font, v2f((f32)Size.X - 200, TextPosY), v4f(0.009, 0.00, 0, 1.0), TextHeight, Str8("LF"));

	cursor *Cursor = Cursors;
	if(!Cursor->Next)
	{
		int Line = E_LineFromBufferIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int Index = IndexFromLine(Buffer->String, Line);
		int Column = Cursor->Selection.Y - LineStart;
		D_Str8(Draw, Arena,
				   Font, v2f((f32)Size.X - 1500, TextPosY),
				   v4f(0.009, 0.00, 0, 1.0),
				   TextHeight, Str8PushF(Arena, "?u32:?u32 | Index: ?u32:?u32 | ?v2f", Line, Column, Cursor->Selection.Y, Index, Cursor->Position));
	}
	else
	{
		int Count = 0;
		for(;Cursor;Cursor = Cursor->Next)
		{
			Count += 1;
		}
		D_Str8(Draw, Arena, Font, v2f((f32)Size.X - 1000, TextPosY),
				   v4f(0.009, 0.00, 0, 1.0), TextHeight,
				   Str8PushF(Arena, "?u32 SELECTIONS", Count));
	}

	f32 PointSize = 20.0f;
	f32 PointPosY = (StatusBarHeight - 0.0f) / 2.0f;
	v3f Position = v3f(Size.X - 100, PointPosY, 0.0);
	if(!Buffer->EditedSinceLastSave)
	{
		D_PointsArray(Draw, Arena, &Position, 1, 20, v4f(0.0, 0.6, 0, 1));
	}
	else
	{
		D_PointsArray(Draw, Arena, &Position, 1, 20, v4f(1, 0.2, 0, 1));
	}

}
