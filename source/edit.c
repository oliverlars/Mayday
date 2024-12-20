#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>


void E_AdvanceCursorXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N)
{
	int Index = E_BufferIndexFromCursor(E, Buffer, Cursor);

	if(N > 0)
	{
		for(int I = 0; I < N; I++)
		{
			if(Buffer->String.Bytes[Index + I] == '\n')
			{
				Cursor->Column += 1;
				Cursor->StickyX = 0;
			}
			else
			{
				Cursor->StickyX += 1;
			}
		}
	}
	else if(N < 0)
	{
		int NextIndex = ClampS32(Index + N, 0, Buffer->String.Size-1);
		int Columns = 0;

		int LineStart = 0;

		for(int I = 0; I < NextIndex; I++)
		{
			if(Buffer->String.Bytes[I] == '\n')
			{
				LineStart = I + 1;
				Columns++;
			}
		}
	
		Cursor->Column = Columns;
		Cursor->StickyX = NextIndex - LineStart;
	}
}

int E_BufferIndexFromCursor(edit_context *E, editable_buffer *Buffer, cursor *Cursor)
{
	int Result = 0;
	int Column = 0;
	int BufferIndex = 0;
	for(; BufferIndex < Buffer->String.Size; BufferIndex++)
	{
		if(Cursor->Column == Column) break;
		if(Buffer->String.Bytes[BufferIndex] == '\n') Column++;
	}

	int LengthOfLine = 0;
	for(; LengthOfLine + BufferIndex < Buffer->String.Size; LengthOfLine++)
	{
		if(Buffer->String.Bytes[LengthOfLine + BufferIndex] == '\n') break;
	}
	return BufferIndex + Min(Cursor->StickyX, LengthOfLine);
}

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert)
{
	if(!E->Temp)
	{
		E->Temp = ArenaPushZero(&E->Arena, MegaBytes(1));
	}

	int At = E_BufferIndexFromCursor(E, Buffer, Cursor);

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

	int At = E_BufferIndexFromCursor(E, &Buffer, Cursor);
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
	int At = E_BufferIndexFromCursor(E, &Buffer, Cursor); 
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

int E_DistanceFromPreviousNewLine(edit_context *E, cursor *Cursor,
								  editable_buffer Buffer)
{
	int At = E_BufferIndexFromCursor(E, &Buffer, Cursor); 
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