#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>

v2i E_GetSortedSelection(v2i Selection)
{
	v2i Result;
	Result.X = Min(Selection.X, Selection.Y);
	Result.Y = Max(Selection.X, Selection.Y);
	return Result;
}

void E_DeleteTo(edit_context *Edit, draw_context *Draw, d_font Font, cursor *Cursor, editable_buffer *Buffer, edit_movement Movement)
{
	// NOTE(Oliver): For each deletion, every subsequent cursor should be 
	// offset by the previous cursors selection range
	int PerCursorOffset = 0;
	for(;Cursor; Cursor = Cursor->Next)
	{
		if(Cursor->Selection.X != Cursor->Selection.Y)
		{
			v2i Selection = E_GetSortedSelection(Cursor->Selection);
			Selection = v2i(Max(Selection.X - PerCursorOffset, 0), Max(Selection.Y - PerCursorOffset, 0));
			E_RemoveTextFromSelection(Edit, Buffer, Selection);
			// HACK(Oliver): Does this need to be moved?
			Cursor->Selection = v2i(Selection.X, Selection.X);

			PerCursorOffset += Selection.Y - Selection.X;
		}
		else
		{
			v2i Selection = E_GetSortedSelection(Cursor->Selection);
			E_RemoveTextFromSelection(Edit, Buffer, v2i(Selection.X - 1, Selection.Y));
			Cursor->Selection = v2i(Max(Selection.X - 1, 0), Max(Selection.X - 1, 0));

		}
	}
}

void E_RemoveTextFromSelection(edit_context *Edit, editable_buffer *Buffer, v2i Selection)
{
	if(!Edit->Temp)
	{
		Edit->Temp = ArenaPushZero(&Edit->Arena, MegaBytes(1));;
	}
	Selection = E_GetSortedSelection(Selection);
	MemoryCopy(Edit->Temp, Buffer->String.Bytes, Selection.X);
	MemoryCopy(Edit->Temp + Selection.X, Buffer->String.Bytes + Selection.Y, Buffer->String.Size - Selection.Y);
	MemoryCopy(Buffer->String.Bytes, Edit->Temp, Buffer->String.Size - (Selection.Y - Selection.X));
}

void E_MoveCursors(edit_context *Edit, draw_context *Draw,
				   d_font Font, cursor *Cursor, editable_buffer *Buffer,
				   edit_movement Movement, edit_move_type MoveType)
{
	for(;Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = Cursor->Selection;
		b32 IsInSelection = Cursor->Selection.X != Cursor->Selection.Y;
		if(Movement == E_MOVE_LEFT && IsInSelection && MoveType == E_MOVE_NOT_SELECTING)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			Selection = v2i(Selection.X, Selection.X);
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_RIGHT && IsInSelection && MoveType == E_MOVE_NOT_SELECTING)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			Selection = v2i(Selection.Y, Selection.Y);
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_LEFT)
		{
			Selection.X -= 1;
			Selection.Y -= 1;
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_RIGHT)
		{
			Selection.X += 1;
			Selection.Y += 1;
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_DOWN)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			int Index = Selection.Y;
			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(Edit, Index, Buffer);

			int LineBelowStart = Index + DistanceFromNextNewLine + 1;
			int LineBelowEnd = LineBelowStart + E_DistanceFromNextNewLine(Edit, LineBelowStart, Buffer);

			if(Cursor->VisualX == 0)
			{
				Cursor->VisualX = E_VisualXFromIndex(Edit, Draw, Font, Selection.Y, Buffer);
			}
			int IndexMaybePastLineEnd = E_IndexFromVisualXAndLineStart(Edit, Draw, Font, Buffer, Cursor->VisualX, LineBelowStart);
			Index = Min(LineBelowEnd, IndexMaybePastLineEnd);
			Selection = v2i(Index, Index);
		}
		else if(Movement == E_MOVE_UP)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			int Index = Selection.Y;
			int DistancePreviousNewLine = E_DistanceFromPreviousNewLine(Edit, Index, Buffer);
			
			int LineAboveEnd = Index - DistancePreviousNewLine - 1;
			int LineAboveStart = LineAboveEnd - E_DistanceFromPreviousNewLine(Edit, LineAboveEnd, Buffer);

			if(Cursor->VisualX == 0)
			{
				Cursor->VisualX = E_VisualXFromIndex(Edit, Draw, Font, Selection.Y, Buffer);
			}
			int IndexMaybePastLineEnd = E_IndexFromVisualXAndLineStart(Edit, Draw, Font, Buffer, Cursor->VisualX, LineAboveStart);
			Index = Min(LineAboveEnd, IndexMaybePastLineEnd);
			Selection = v2i(Index, Index);
		}
		if(MoveType == E_MOVE_SELECTING)
		{
			Cursor->Selection.Y = ClampS32(Selection.Y, 0, Buffer->String.Size);
		}
		else if(MoveType == E_MOVE_NOT_SELECTING)
		{
			Cursor->Selection.X = ClampS32(Selection.X, 0, Buffer->String.Size);
			Cursor->Selection.Y = ClampS32(Selection.Y, 0, Buffer->String.Size);
		}
	}
}

// void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert)
// {
// 	if(!E->Temp)
// 	{
// 		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
// 	}

// 	int At = Cursor->End;

// 	//TODO(Oliver): Not implemented yet
// 	assert(Buffer->String.Size + TextToInsert.Size < MegaBytes(1));
// 	assert(E->Temp);
	
// 	MemoryCopy(E->Temp, Buffer->String.Bytes, At);
// 	MemoryCopy(E->Temp + At, TextToInsert.Bytes, TextToInsert.Size);
// 	MemoryCopy(E->Temp + At + TextToInsert.Size, Buffer->String.Bytes + At, Buffer->String.Size - At);

// 	MemoryCopy(Buffer->String.Bytes, E->Temp, Buffer->String.Size + TextToInsert.Size);


// 	E_AdvanceCursorsXByN(E, Cursor, Buffer, TextToInsert.Size);
// 	Cursor->Start = Cursor->End;

// 	Buffer->String.Size = ClampS32(Buffer->String.Size + TextToInsert.Size, 0, MegaBytes(1));
// }

void E_RemoveSelectedTextAtCursors(edit_context *E, cursor *Cursor, editable_buffer *Buffer)
{
	if(!E->Temp)
	{
		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
	}

	assert(E->Temp);

	int BytesCopiedSoFar = 0;

	int PreviousEnd = 0;

	int TotalDistanceToMoveCursors = 0;

	for(; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		int SegmentSize = Max(Selection.X - PreviousEnd, 0);
		MemoryCopy(E->Temp + BytesCopiedSoFar, Buffer->String.Bytes + PreviousEnd, SegmentSize);
		BytesCopiedSoFar += SegmentSize;
		PreviousEnd = Selection.Y;

		Cursor->Selection = Selection;

		TotalDistanceToMoveCursors += (Selection.Y - Selection.X);
		Cursor->Selection.Y -= TotalDistanceToMoveCursors;
		
		Cursor->Selection.X = Cursor->Selection.Y;
	}

	int TailLength = Buffer->String.Size - PreviousEnd;
	MemoryCopy(E->Temp + BytesCopiedSoFar, Buffer->String.Bytes + PreviousEnd, TailLength);
	BytesCopiedSoFar += TailLength;
	MemoryCopy(Buffer->String.Bytes, E->Temp, BytesCopiedSoFar);

	Buffer->String.Size = BytesCopiedSoFar;
}

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2f Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer.HeightInPixels);

	f32 StartX = Position.X;
	v2f Result = Position;

	int At = Index;
	for(int I = 0; I < Buffer.String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = Buffer.String.Bytes[I];

		if(I == At)
		{
			break;
		}

		if(CodePoint == '\n')
		{
			Result.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Result.X = StartX;
			continue;
		}

		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, Buffer.HeightInPixels, CodePoint);

		Result.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}
	Result.Y += Font.Descent*Scale;
	return Result;
}

int E_DistanceFromNextNewLine(edit_context *E, int Index, editable_buffer *Buffer)
{
	int At = Index; 
	int I = At;
	for(;I < Buffer->String.Size; I++)
	{
		if(Buffer->String.Bytes[I] == '\n')
		{
			break;
		}
	}
	return I - At;
}

int E_DistanceFromPreviousNewLine(edit_context *E, int Index, editable_buffer *Buffer)
{
	int At = Index - 1;
	int I = At;
	for(;I >= 0; I--)
	{
		if(Buffer->String.Bytes[I] == '\n')
		{
			break;
		}
	}
	return At - I;
}


int E_LineFromBufferIndex(edit_context *E_Context, int Index, editable_buffer *Buffer)
{
	int Line = 0;
	for(int I = 0; I < Index; I++)
	{
		if(Buffer->String.Bytes[I] == '\n') Line++;
	}

	return Line;
}

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D, d_font Font,
							   editable_buffer *Buffer, int RangeStart, int RangeEnd)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	int MaxWidth = 0;
	int X = 0;
	for(int I = RangeStart; I < RangeEnd; I++)
	{
		u32 CodePoint =  Buffer->String.Bytes[I];
		
		if(CodePoint == '\n')
		{
			X = 0;
			continue;
		}

		f32 TabMultiplier = 1.0;
		if(CodePoint == '\t')
		{

			TabMultiplier *= 4.0;
		}
		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, Buffer->HeightInPixels, CodePoint);

		X += Glyph->AdvanceX * Scale * TabMultiplier;
		MaxWidth = Max(X, MaxWidth);
	}

	return MaxWidth;
}

int E_IndexFromVisualXAndLineStart(edit_context *E, draw_context *D, d_font Font,
								   editable_buffer *Buffer, int VisualX, int LineStart)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	int X = 0;
	int Index = LineStart;
	for(;Index < Buffer->String.Size; Index++)
	{
		u32 CodePoint =  Buffer->String.Bytes[Index];
		
		if(CodePoint == '\n' || X >= VisualX)
		{
			break;
		}

		f32 TabMultiplier = 1.0;
		if(CodePoint == '\t')
		{

			TabMultiplier *= 4.0;
		}
		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, Buffer->HeightInPixels, CodePoint);

		X += Glyph->AdvanceX * Scale * TabMultiplier;
	}

	return Index;
}

int E_VisualXFromIndex(edit_context *Edit, draw_context *Draw,
					   d_font Font, int Index, editable_buffer *Buffer)
{
	int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(Edit, Index, Buffer);
	int LineStart = Index - DistanceFromPreviousNewLine;
	return E_GetMaxTextWidthFromRange(Edit, Draw, Font,  Buffer, LineStart, Index);
}

// void E_MoveCursorVertically(edit_context *E, draw_context *D, d_font Font, int VisualX, int Index, editable_buffer *Buffer, int N)
// {
// 	int Result = Index;
// 	if(N > 0)
// 	{
// 			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Index, Buffer);
// 			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Index, Buffer);	
// 			if(VisualX == 0)
// 			{
// 				int RangeStart = Index - DistanceFromPreviousNewLine;
// 				int RangeEnd =  Index;
// 				VisualX = E_GetMaxTextWidthFromRange(E, D, Font, Cursor, Buffer, RangeStart, RangeEnd);
// 			}
// 			Cursor->End += DistanceFromNextNewLine + 1;
// 			DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Cursor, Buffer);
// 			int RangeEndFromVisualX = E_RangeEndFromWidth(E, D, Font, Cursor, Buffer, Cursor->VisualX, Cursor->End);
// 			Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);
// 	}
// 	else if(N < 0)
// 	{
// 		for(;Cursor; Cursor = Cursor->Next)
// 		{
// 			int CursorEnd = Cursor->End;
// 			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Cursor, Buffer);
// 			if(Cursor->VisualX == 0)
// 			{
// 				int RangeStart = Cursor->End - DistanceFromPreviousNewLine;
// 				int RangeEnd = Cursor->End;
// 				Cursor->VisualX = E_GetMaxTextWidthFromRange(E, D, Font, Cursor, Buffer, RangeStart, RangeEnd);
// 			}
// 			Cursor->End -= DistanceFromPreviousNewLine + 1;
// 			DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Cursor, Buffer);
			
// 			Cursor->End -= DistanceFromPreviousNewLine;
// 			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Cursor, Buffer);

// 			int RangeEndFromVisualX = E_RangeEndFromWidth(E, D, Font, Cursor, Buffer, Cursor->VisualX, Cursor->End);
// 			Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);

// 			Cursor->End = Max(Cursor->End, 0);
// 		}	
// 	}
// }


cursor *E_LastCursor(edit_context *Edit, cursor *Cursor)
{
	for(;Cursor->Next; Cursor = Cursor->Next);
	return Cursor;
}

void E_AppendCursor(edit_context *Edit, cursor *Cursor, cursor NextCursor)
{
	assert(Cursor);
	Cursor = E_LastCursor(Edit, Cursor);

	Cursor->Next = ArenaPushTypeZero(&Edit->Arena, cursor);
	NextCursor.Previous = Cursor;
	Cursor->Next[0] = NextCursor;
}

void PushUndoAction(edit_context *E, cursor *Cursor, undo_type Type, str8 InsertedOrRemovedText)
{
	undo_stack *Action = (undo_stack*)ArenaPushZero(&E->Arena, sizeof(undo_stack));
	Action->Index = Cursor->Selection.X;
	Action->String = Str8PushF(&E->Arena, "?S", InsertedOrRemovedText);
	Action->Type = Type;
	if(!E->NewestAction)
	{
		E->NewestAction = Action;
		E->OldestAction = Action;
	}
	else
	{
		E->NewestAction->Next = Action;
		Action->Previous = E->NewestAction;
		E->NewestAction = Action;
	}
}

undo_stack *PopOffUndoStack(edit_context *E)
{
	assert(E->NewestAction);
	undo_stack *Result = E->NewestAction;
	if(E->NewestAction->Previous)
	{
		E->NewestAction = E->NewestAction->Previous;
	}
	else
	{
		E->NewestAction = Null;
		E->OldestAction = Null;
	}
	return Result;
}