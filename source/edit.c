#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>


void E_AdvanceCursorXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N)
{
	Cursor->End += N;
	Cursor->Start += N;
}

int E_BufferIndexFromCursor(edit_context *E, editable_buffer *Buffer, cursor *Cursor)
{
	return Cursor->End;
}

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


	E_AdvanceCursorXByN(E, Cursor, Buffer, TextToInsert.Size);

	Buffer->String.Size += TextToInsert.Size;
}

void E_RemoveTextFromTextRange(edit_context *E, cursor *Cursor, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd)
{
	if(!E->Temp)
	{
		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
	}

	assert(E->Temp);

	E_AdvanceCursorXByN(E, Cursor, Buffer, -(RangeEnd-RangeStart));
	
	MemoryCopy(E->Temp, Buffer->String.Bytes, RangeStart);
	MemoryCopy(E->Temp + RangeStart, Buffer->String.Bytes + RangeEnd, Buffer->String.Size - RangeEnd);

	MemoryCopy(Buffer->String.Bytes, E->Temp, Buffer->String.Size - (RangeEnd-RangeStart));

	Buffer->String.Size -= (RangeEnd-RangeStart);
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
							  editable_buffer Buffer)
{
	int At = Cursor->End; 
	int I = At;
	for(;I < Buffer.String.Size; I++)
	{
		if(Buffer.String.Bytes[I] == '\n')
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
								  editable_buffer Buffer)
{
	int At = Cursor->End - 1;
	int I = At;
	for(;I >= 0; I--)
	{
		if(Buffer.String.Bytes[I] == '\n')
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

void E_AppendCursor(edit_context *E_Context, cursor *Cursor, cursor NextCursor)
{
	assert(Cursor);

	for(;Cursor->Next; Cursor = Cursor->Next);

	Cursor->Next = ArenaPushTypeZero(&E_Context->Arena, cursor);
	NextCursor.Previous = Cursor;
	Cursor->Next[0] = NextCursor;
}