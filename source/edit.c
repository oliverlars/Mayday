#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert)
{
	if(!E->Temp)
	{
		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
	}

	int At = Cursor->End;

	//TODO(Oliver): Not implemented yet
	assert(Buffer->String.Size + TextToInsert.Size < MegaBytes(1));
	assert(E->Temp);
	
	MemoryCopy(E->Temp, Buffer->String.Bytes, At);
	MemoryCopy(E->Temp + At, TextToInsert.Bytes, TextToInsert.Size);
	MemoryCopy(E->Temp + At + TextToInsert.Size, Buffer->String.Bytes + At, Buffer->String.Size - At);

	MemoryCopy(Buffer->String.Bytes, E->Temp, Buffer->String.Size + TextToInsert.Size);


	E_AdvanceCursorsXByN(E, Cursor, Buffer, TextToInsert.Size);

	Buffer->String.Size = ClampS32(Buffer->String.Size + TextToInsert.Size, 0, MegaBytes(1));
}

void E_RemoveTextFromTextRange(edit_context *E, cursor *Cursor, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd)
{
	if(!E->Temp)
	{
		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
	}

	assert(E->Temp);
	
	int Count = RangeEnd - RangeStart;

	int BytesCopiedSoFar = 0;

	int PreviousEnd = 0;
	for(; Cursor; Cursor = Cursor->Next)
	{
		int Start = Cursor->Start;
		int End = Cursor->End;
		int UpTo = Buffer->String.Size;
		if(Cursor->Next)
		{
			UpTo = Min(Cursor->Next->Start, Cursor->Next->End);
		}
		if(End < Start)
		{
			int Temp = Start;
			Start = End;
			End = Temp;
		}

		int SegmentSize = Start - PreviousEnd;

		MemoryCopy(E->Temp + BytesCopiedSoFar, Buffer->String.Bytes + PreviousEnd, SegmentSize);
		BytesCopiedSoFar += SegmentSize;
		PreviousEnd = End;
	}

	int TailLength = Buffer->String.Size - PreviousEnd;
	MemoryCopy(E->Temp + BytesCopiedSoFar, Buffer->String.Bytes + PreviousEnd, TailLength);
	BytesCopiedSoFar += TailLength;
	MemoryCopy(Buffer->String.Bytes, E->Temp, BytesCopiedSoFar);
	
	Buffer->String.Size = BytesCopiedSoFar;
}

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									cursor *Cursor,
									editable_buffer Buffer,
									d_font Font,
									v2f Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer.HeightInPixels);

	f32 StartX = Position.X;
	v2f Result = Position;

	int At = Cursor->End;
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

int E_DistanceFromNextNewLine(edit_context *E, cursor *Cursor,
							  editable_buffer *Buffer)
{
	int At = Cursor->End; 
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


int E_LineFromBufferIndex(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer)
{
	int Line = 0;
	for(int I = 0; I < Cursor->End; I++)
	{
		if(Buffer->String.Bytes[I] == '\n') Line++;
	}

	return Line;
}

int E_DistanceFromPreviousNewLine(edit_context *E, cursor *Cursor,
								  editable_buffer *Buffer)
{
	int At = Cursor->End - 1;
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

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D, d_font Font, cursor *Cursor,
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

int E_RangeEndFromWidth(edit_context *E, draw_context *D, d_font Font,
						cursor *Cursor, editable_buffer *Buffer, int Width, int RangeStart)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	int X = 0;
	int RangeEnd = RangeStart;
	for(;RangeEnd < Buffer->String.Size; RangeEnd++)
	{
		u32 CodePoint =  Buffer->String.Bytes[RangeEnd];
		
		if(CodePoint == '\n' || X >= Width)
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

	return RangeEnd;
}

void E_AdvanceCursorsYByN(edit_context *E, draw_context *D, d_font Font, cursor *Cursor, editable_buffer *Buffer, int N)
{
	if(N > 0)
	{
		for(;Cursor; Cursor = Cursor->Next)
		{
			int CursorEnd = Cursor->End;
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Cursor, Buffer);
			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Cursor, Buffer);	
			if(Cursor->VisualX == 0)
			{
				int RangeStart = Cursor->End - DistanceFromPreviousNewLine;
				int RangeEnd = Cursor->End;
				Cursor->VisualX = E_GetMaxTextWidthFromRange(E, D, Font, Cursor, Buffer, RangeStart, RangeEnd);
			}
			Cursor->End += DistanceFromNextNewLine + 1;
			DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Cursor, Buffer);

			int RangeEndFromVisualX = E_RangeEndFromWidth(E, D, Font, Cursor, Buffer, Cursor->VisualX, Cursor->End);
			Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);
		}
	}
	else if(N < 0)
	{
		for(;Cursor; Cursor = Cursor->Next)
		{
			int CursorEnd = Cursor->End;
			int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Cursor, Buffer);
			if(Cursor->VisualX == 0)
			{
				int RangeStart = Cursor->End - DistanceFromPreviousNewLine;
				int RangeEnd = Cursor->End;
				Cursor->VisualX = E_GetMaxTextWidthFromRange(E, D, Font, Cursor, Buffer, RangeStart, RangeEnd);
			}
			Cursor->End -= DistanceFromPreviousNewLine + 1;
			DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(E, Cursor, Buffer);
			
			Cursor->End -= DistanceFromPreviousNewLine;
			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(E, Cursor, Buffer);

			int RangeEndFromVisualX = E_RangeEndFromWidth(E, D, Font, Cursor, Buffer, Cursor->VisualX, Cursor->End);
			Cursor->End += Min(DistanceFromNextNewLine, RangeEndFromVisualX - Cursor->End);
		}	
	}
}

void E_AdvanceCursorsXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N)
{
	for(;Cursor; Cursor = Cursor->Next)
	{
		Cursor->VisualX = 0;
		Cursor->End = ClampS32(Cursor->End + N, 0, Buffer->String.Size);
	}
}

void E_AdvanceSingleCursorXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N)
{
	Cursor->VisualX = 0;
	Cursor->End = ClampS32(Cursor->End + N, 0, Buffer->String.Size);	
}

void E_AppendCursor(edit_context *E_Context, cursor *Cursor, cursor NextCursor)
{
	assert(Cursor);

	for(;Cursor->Next; Cursor = Cursor->Next);

	Cursor->Next = ArenaPushTypeZero(&E_Context->Arena, cursor);
	NextCursor.Previous = Cursor;
	Cursor->Next[0] = NextCursor;
}