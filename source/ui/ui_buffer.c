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
#include <signal.h>



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

void UI_BufferUpdate(
	ui_context *UI, draw_context *Draw,
	edit_context *Edit, d_font Font,
	m_arena *Arena, editable_buffer *Buffer,
	str8 BufferPath, v2i Size)
{

	int WindowHeight = Size.Y;
	int WindowWidth = Size.X;

	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);

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

	if(UI_DoubleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = Edit->Cursors.Last;
		cursor *Cursor = LastCursor;

		v2i InitialSelection = Cursor->Selection;
		E_MoveCursor(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
		v2i Selection = Cursor->Selection;
		Cursor->Selection = InitialSelection;
		E_MoveCursor(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
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

		E_CollapseToFirst(&Edit->Cursors);
		cursor *LastCursor = Edit->Cursors.Last;
		cursor *Cursor = LastCursor;

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
		Edit->Cursors.First[0].Selection = v2i(0, Buffer->String.Size);

		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, Range, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = Edit->Cursors.Last;
		cursor *Cursor = LastCursor;

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_LeftMouseWentDownWithModifiers(UI, SAPP_MODIFIER_ALT))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = Edit->Cursors.Last;
		cursor *Cursor = LastCursor;
		cursor NextCursor = Cursor[0];
		NextCursor.Selection = v2i(Index, Index);
		E_AppendCursor(Edit->Arena, &Edit->Cursors, NextCursor);
	}
	else if(UI_LeftMouseWentDown(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, Range, BufferPosition,
									UI->MousePosition);
		E_CollapseToFirst(&Edit->Cursors);
		cursor *Cursor = Edit->Cursors.First;
		Cursor->Selection = v2i(Index, Index);

	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_ESCAPE, 0))
	{
		E_CollapseToFirst(&Edit->Cursors);
		cursor *Cursor = Edit->Cursors.First;
		Cursor->Selection.X = Cursor->Selection.Y;
	}


	if(UI_KeyDown(UI, SAPP_KEYCODE_ENTER, 0))
	{
		E_InsertTo(Edit, &Edit->Cursors, Buffer, Str8("\n"));

		int TotalWhiteSpaceInserted = 0;
		for(cursor *Cursor = Edit->Cursors.First; Cursor; Cursor = Cursor->Next)
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
		// Collapse all cursors down to a single cursor spanning the whole buffer.
		cursor *Cursor = Edit->Cursors.Last;
		Cursor->Next = Null;
		Cursor->Previous = Null;
		Edit->Cursors.First = Cursor;
		Edit->Cursors.Last = Cursor;
		Edit->Cursors.Count = 1;
		Cursor->Selection.X = 0;
		Cursor->Selection.Y = Buffer->String.Size;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_D, SAPP_MODIFIER_SUPER))
	{
		str8 StringToFind = {};
		cursor *Cursor = Edit->Cursors.Last;
		if(Cursor->Selection.X == Cursor->Selection.Y)
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, Range, BufferPosition,
										UI->MousePosition);
			v2i InitialSelection = Cursor->Selection;
			E_MoveCursor(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
			v2i Selection = Cursor->Selection;
			Cursor->Selection = InitialSelection;
			E_MoveCursor(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
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
					E_AppendCursor(Edit->Arena, &Edit->Cursors, NextCursor);
					break;
				}
			}

		}
	}

	if(Buffer->Search)
	{
		assert(Edit->Temp);
		ui_textbox_signal Signal = UI_TextBox(Arena, Font, v2f(sapp_widthf()/2 - 100, sapp_heightf() - 300), 200, UI, Draw, Edit, 100, Str8("BufferSearchBox"), &Edit->SearchBox);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_F, SAPP_MODIFIER_SUPER))
	{
		UI->Active = UI_IDFromString(Str8("BufferSearchBox"));
		Buffer->Search = True;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_TAB, 0))
	{
		E_InsertTo(Edit, &Edit->Cursors, Buffer, Str8("\t"));
	}

	// Split selection(s) into one cursor per line at line-end, then collapse.
	// Matches VS Code "Add Cursors to Line Ends" (Shift+Alt+I).
	if(UI_KeyDown(UI, SAPP_KEYCODE_I, SAPP_MODIFIER_SHIFT | SAPP_MODIFIER_ALT))
	{
		cursor *LastOriginal = Edit->Cursors.Last;
		cursor *C = Edit->Cursors.First;
		b32 Done = False;
		while(!Done)
		{
			Done = (C == LastOriginal);
			cursor *NextOriginal = C->Next;

			v2i Selection = E_GetSortedSelection(C->Selection);
			u8 *Bytes = Buffer->String.Bytes;
			int Size = Buffer->String.Size;

			// Find end of the first line in the selection by scanning forward.
			int FirstLineEnd = Selection.X;
			while(FirstLineEnd < Size && Bytes[FirstLineEnd] != '\n')
			{
				FirstLineEnd++;
			}

			if(FirstLineEnd >= Selection.Y)
			{
				// Single-line (or empty) selection: just collapse.
				C->Selection = v2i(Selection.Y, Selection.Y);
			}
			else
			{
				// Collapse this cursor to the end of its first line.
				C->Selection = v2i(FirstLineEnd, FirstLineEnd);

				// First pass: count additional cursors needed. Direct newline
				// scan — no reliance on E_LineEndFromIndex, which can return
				// a value less than its input on empty lines (infinite loop).
				int ExtraCount = 0;
				{
					int Idx = FirstLineEnd + 1;
					while(Idx <= Selection.Y && Idx < Size)
					{
						int LineEnd = Idx;
						while(LineEnd < Size && Bytes[LineEnd] != '\n')
						{
							LineEnd++;
						}
						ExtraCount++;
						if(LineEnd >= Selection.Y) break;
						Idx = LineEnd + 1;
					}
					// Trailing-no-newline case: selection ends at EOF past last '\n'.
					if(Idx >= Size && Selection.Y == Size && Idx <= Selection.Y)
					{
						ExtraCount++;
					}
				}

				if(ExtraCount > 0)
				{
					// Batch allocate + link. One arena push instead of N.
					cursor *Block = E_AppendCursorsBulk(Edit->Arena, &Edit->Cursors, ExtraCount);

					int I = 0;
					int Idx = FirstLineEnd + 1;
					while(Idx <= Selection.Y && Idx < Size && I < ExtraCount)
					{
						int LineEnd = Idx;
						while(LineEnd < Size && Bytes[LineEnd] != '\n')
						{
							LineEnd++;
						}
						int EffectiveEnd = Min(LineEnd, Selection.Y);

						// Copy cosmetic fields from the originating cursor but
						// preserve the pre-linked Next/Previous pointers.
						cursor *Prev = Block[I].Previous;
						cursor *Nxt  = Block[I].Next;
						Block[I] = *C;
						Block[I].Previous = Prev;
						Block[I].Next = Nxt;
						Block[I].Selection = v2i(EffectiveEnd, EffectiveEnd);

						I++;
						if(LineEnd >= Selection.Y) break;
						Idx = LineEnd + 1;
					}
					// Trailing-no-newline case.
					if(I < ExtraCount)
					{
						cursor *Prev = Block[I].Previous;
						cursor *Nxt  = Block[I].Next;
						Block[I] = *C;
						Block[I].Previous = Prev;
						Block[I].Next = Nxt;
						Block[I].Selection = v2i(Selection.Y, Selection.Y);
					}
				}
			}

			C = NextOriginal;
		}
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

					E_MoveCursors(Edit, Draw, Font, &Edit->Cursors, Buffer, Movement, MoveType);
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

					E_MoveCursors(Edit, Draw, Font, &Edit->Cursors, Buffer, Movement, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_UP:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER && Event.modifiers & SAPP_MODIFIER_ALT)
					{
						cursor *LastCursor = Edit->Cursors.Last;
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit->Arena, &Edit->Cursors, NextCursor);
						E_MoveCursor(Edit, Draw, Font, Edit->Cursors.Last, Buffer, E_MOVE_UP, MoveType);
					}
					else
					{
						E_MoveCursors(Edit, Draw, Font, &Edit->Cursors, Buffer, E_MOVE_UP, MoveType);
					}
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_DOWN:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER && Event.modifiers & SAPP_MODIFIER_ALT)
					{
						cursor *LastCursor = Edit->Cursors.Last;
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit->Arena, &Edit->Cursors, NextCursor);
						E_MoveCursor(Edit, Draw, Font, Edit->Cursors.Last, Buffer, E_MOVE_DOWN, MoveType);
					}
					else
					{
						E_MoveCursors(Edit, Draw, Font, &Edit->Cursors, Buffer, E_MOVE_DOWN, MoveType);
					}
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_BACKSPACE:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						cursor *Cursor = Edit->Cursors.Last;
						E_MoveCursor(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_SELECTING);
					}
					E_DeleteTo(Edit, &Edit->Cursors, Buffer);
					
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
		E_InsertTo(Edit, &Edit->Cursors, Buffer, TextToInsert);
	}
}

void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font, d_font NumbersFont,
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

	// Number column: black background
	D_Quad(Draw, v2f(0, 0), NumberOffsetX - 5.0f, (f32)WindowHeight + LineHeight, v4f(0.0, 0.0, 0.0, 1.0));

	f32 CodeScale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	f32 GutterWidth = NumberOffsetX - 5.0f;
	int MainCursorLine = E_LineFromBufferIndex(Edit, Edit->Cursors.First[0].Selection.Y, Buffer);
	for(int I = FirstVisibleLine; I <= LastVisibleLine; I++)
	{
		str8 NumStr = Str8PushF(Arena, "?u32", I + 1);
		f32 NumWidth = (f32)UI_GetTextWidth(NumbersFont, Draw, NumStr, LineHeight);
		f32 CenteredX = (GutterWidth - NumWidth) / 2.0f;

		if(MainCursorLine == I)
		{
			// Active line: red background, black text — centered in gutter
			f32 QuadY = NumbersPosition.Y + Font.Descent * CodeScale;
			D_Quad(Draw, v2f(0, QuadY), GutterWidth, LineHeight, v4f(0.753, 0.036, 0.0, 1.0));
			D_Str8(Draw, NumStr, .Font = NumbersFont,
			.Position = V3FromV2(v2f(CenteredX, NumbersPosition.Y)),
				.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = LineHeight);
		}
		else
		{
			// Non-active line: red text on black — centered in gutter
			D_Str8(Draw, NumStr, .Font = NumbersFont,
			.Position = V3FromV2(v2f(CenteredX, NumbersPosition.Y)),
				.Colour = v4f(0.753, 0.036, 0.0, 1.0), .HeightInPixels = LineHeight);
		}
		NumbersPosition.Y -= LineHeight;
	}

	f32 GlyphScale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);

	// Build highlight array marking selected characters across all cursors
	b8 *SelHighlight = ArenaPushArrayZero(Arena, b8, SubString.Size + 1);
	for(cursor *Cursor = Edit->Cursors.First; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		if(Selection.X == Selection.Y) continue;

		// Cull cursors whose selection is entirely outside the visible byte
		// range — skips the O(buffer) line-number lookups for off-screen ones.
		if(Selection.Y < StartIndex || Selection.X > EndIndex) continue;

		// Draw black quads only behind selected text (not full line width)
		int FirstSelLine = E_LineFromBufferIndex(Edit, Selection.X, Buffer);
		int LastSelLine = E_LineFromBufferIndex(Edit, Selection.Y, Buffer);
		int VisFirstSel = Max(FirstSelLine, FirstVisibleLine);
		int VisLastSel = Min(LastSelLine, LastVisibleLine);
		for(int L = VisFirstSel; L <= VisLastSel; L++)
		{
			f32 LineY = BufferPosition.Y - (f32)(L - FirstVisibleLine) * LineHeight;
			LineY += Font.Descent * GlyphScale;

			// Find a reference index on this line to locate line boundaries
			int RefIdx;
			if(L == FirstSelLine) RefIdx = Selection.X;
			else if(L == LastSelLine) RefIdx = Selection.Y;
			else if(L > 0) RefIdx = IndexFromLine(Buffer->String, L - 1) + 1;
			else RefIdx = 0;

			int LineStart = E_LineStartFromIndex(Edit, RefIdx, Buffer);
			int LineEnd = E_LineEndFromIndex(Edit, RefIdx, Buffer);

			// Clamp selection to this line
			int SelStartOnLine = Max(Selection.X, LineStart);
			int SelEndOnLine = Min(Selection.Y, LineEnd);

			// Get visual X positions for selection start and end
			v2f StartPos = E_GetVisualPositionFromEditableBuffer(
				Edit, Draw, SelStartOnLine, Buffer[0], Font, Range, BufferPosition);
			v2f EndPos = E_GetVisualPositionFromEditableBuffer(
				Edit, Draw, SelEndOnLine, Buffer[0], Font, Range, BufferPosition);

			f32 QuadX = StartPos.X;
			f32 QuadWidth = EndPos.X - StartPos.X;

			// If selection reaches end of line, extend to window edge
			if(Selection.Y >= LineEnd)
				QuadWidth = sapp_widthf() - QuadX;

			if(QuadWidth > 0)
				D_Quad(Draw, v2f(QuadX, LineY),
					   QuadWidth, LineHeight, v4f(0.0, 0.0, 0.0, 1.0));
		}

		// Mark selected characters in the highlight array
		int SelStart = Max(Selection.X, StartIndex) - StartIndex;
		int SelEnd = Min(Selection.Y, EndIndex) - StartIndex;
		for(int I = SelStart; I < SelEnd; I++)
		{
			if(I >= 0 && I < (int)SubString.Size)
				SelHighlight[I] = 1;
		}
	}

	// Draw text once — black for normal, red for selected chars
	D_Str8(Draw, SubString, .Font = Font,
		   .Position = V3FromV2(BufferPosition),
		   .Colour = v4f(0.0, 0.0, 0.0, 1),
		   .HighlightColour = v4f(0.753, 0.036, 0.0, 1.0),
		   .HighlightIndices = SelHighlight,
		   .HeightInPixels = Buffer->HeightInPixels);

	// Debug: show insert stats on the right side
	if(Edit->DebugBytesCopied || Edit->DebugBytesInserted)
	{
		f32 DebugX = sapp_widthf() - 700;
		f32 DebugY = sapp_heightf() - 50;
		f32 DebugH = 40;
		f32 DebugSpacing = 50;
		int BufSize = (int)Buffer->String.Size;
		f32 CopyRatio = BufSize > 0 ? (f32)Edit->DebugBytesCopied / (f32)BufSize * 100.0f : 0;
		f32 InsertRatio = BufSize > 0 ? (f32)Edit->DebugBytesInserted / (f32)BufSize * 100.0f : 0;
		f32 AvgPerCursor = Edit->DebugCursorCount > 0 ? (f32)(Edit->DebugBytesCopied + Edit->DebugBytesInserted) / (f32)Edit->DebugCursorCount : 0;

		D_Str8(Draw,
			Str8PushF(Arena, "Bytes copied: ?s32 (?f32%%)", Edit->DebugBytesCopied, CopyRatio),
			.Font = Font, .Position = v3f(DebugX, DebugY, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
		D_Str8(Draw,
			Str8PushF(Arena, "Bytes inserted: ?s32 (?f32%%)", Edit->DebugBytesInserted, InsertRatio),
			.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
		D_Str8(Draw,
			Str8PushF(Arena, "Cursors: ?s32", Edit->DebugCursorCount),
			.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing*2, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
		D_Str8(Draw,
			Str8PushF(Arena, "Avg bytes/cursor: ?f32", AvgPerCursor),
			.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing*3, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
		D_Str8(Draw,
			Str8PushF(Arena, "Buffer size: ?s32", BufSize),
			.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing*4, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
		D_Str8(Draw,
			Str8PushF(Arena, "Time: ?f32 ms", Edit->DebugTimeMs),
			.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing*5, 0),
			.Colour = v4f(0.0, 0.0, 0.0, 1), .HeightInPixels = DebugH);
	}

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
		Edit->Cursors.First[0].Selection.Y = Index;

		Edit->Cursors.First[0].TargetPosition = E_GetVisualPositionFromEditableBuffer(Edit,
																	Draw,
																	Edit->Cursors.First[0].Selection.Y,
																	Buffer[0],
																	Font,
																	Range,
																	BufferPosition);
		Edit->Cursors.First[0].Position = Edit->Cursors.First[0].TargetPosition;
		D_Quad(Draw, Edit->Cursors.First[0].Position, Min(LineHeight/10.0, 2), LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{
		for(cursor *Cursor = Edit->Cursors.First; Cursor; Cursor = Cursor->Next)
		{
			if(Cursor->Selection.Y >= Range.X && Cursor->Selection.Y <= Range.Y)
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
			// Byte-range visibility check — avoids O(buffer) line lookup per cursor.
			if(Cursor->Selection.Y >= Range.X && Cursor->Selection.Y <= Range.Y)
			{
				D_Quad(Draw, Cursor->Position, Min(LineHeight/10.0, 2), LineHeight, v4f(0.3, 0.7, 0, 1));
			}

		}
	}

	if(Buffer->Search)
	{
		UI_TextBox(Arena, Font, v2f(sapp_widthf()/2 - 100, sapp_heightf() - 300), 200, UI, Draw, Edit, 100, Str8("BufferSearchBox"), &Edit->SearchBox);
	}
	else
	{
		UI_BufferUpdate(UI, Draw, Edit, Font, Arena, Buffer, BufferPath, Size);	
	}

	R_PopTransform(Draw->Render);
}

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer, v2i Size)
{

	int Ascent, Descent, LineGap;
	stbtt_GetFontVMetrics(&Font.Info, &Ascent, &Descent, &LineGap);

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels * 1.5);
	float AscentInPixels = Ascent  * Scale;
	float DescentInPixels = Descent * Scale;

	float TextHeight = (AscentInPixels - DescentInPixels);

	f32 StatusBarHeight = Buffer->HeightInPixels * 1.5f;

	float CentreOffset = (AscentInPixels + DescentInPixels) * 0.5f;

	f32 TextPosY  = (StatusBarHeight * 0.5f) - CentreOffset;

	D_Quad(Draw, v2f(0, 0), (f32)Size.X, StatusBarHeight, v4f(0.0, 0.0, 0.0, 1));

	D_Str8(Draw, Str8("LF"), .Font = Font, .Position = v3f((f32)Size.X - 200, TextPosY), .Colour = v4f(0.753, 0.036, 0.0, 1.0), .HeightInPixels = TextHeight);

	cursor *Cursor = Cursors;
	if(!Cursor->Next)
	{
		int Line = E_LineFromBufferIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int Index = IndexFromLine(Buffer->String, Line);
		int Column = Cursor->Selection.Y - LineStart;
		D_Str8(Draw, Str8PushF(Arena, "?u32:?u32 | Index: ?u32:?u32 | ?v2i", Line, Column, Cursor->Selection.Y, Index, v2i(Cursor->Position.X, Cursor->Position.Y)),
				   .Font = Font, .Position = v3f((f32)Size.X - 1500, TextPosY),
				   .Colour = v4f(0.753, 0.036, 0.0, 1.0),
				   .HeightInPixels = TextHeight);
	}
	else
	{
		// O(1) — use the maintained list Count instead of walking.
		int Count = Edit->Cursors.Count;
		D_Str8(Draw, Str8PushF(Arena, "?u32 SELECTIONS", Count),
					.Font = Font, .Position = v3f((f32)Size.X - 1000, TextPosY),
				   .Colour = v4f(0.753, 0.036, 0.0, 1.0), .HeightInPixels = TextHeight);
	}

	f32 PointSize = 20.0f;
	f32 PointPosY = (StatusBarHeight - 0.0f) / 2.0f;
	v3f Position = v3f(Size.X - 200, TextPosY, 0.0);
	if(!Buffer->EditedSinceLastSave)
	{
		D_Str8(Draw, Str8("UNSAVED"), .Font = Font, .Position = Position,
		.HeightInPixels = TextHeight, .Colour = v4f(0.753, 0.036, 0.0, 1.0));
	}
	else
	{
		D_PointsArray(Draw, &Position, 1, 20, v4f(0.753, 0.036, 0.0, 1));
	}

}
