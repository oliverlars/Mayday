#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>


void E_AdvanceCursorXByN(cursor *Cursor, editable_buffer *Buffer, int N)
{
	int Index = E_BufferIndexFromCursor(Buffer, Cursor);
	if(N + Index >= Buffer->String.Size) N = Buffer->String.Size - Index;
	if(N + Index < 0) N = Index;

	if(N > 0)
	{
		for(int I = 0; I < N; I++)
		{
			if(Buffer->String.Bytes[Index + I] == '\n')
			{
				Cursor->StickyX = 0;
				Cursor->Column += 1;
			}
			else
			{
				Cursor->StickyX += 1;
			}
		}
	}
	else
	{
		for(int I = N; I < 0; I++)
		{
			if(Buffer->String.Bytes[Index + I] == '\n')
			{
				Cursor->StickyX = 0;
				Cursor->Column -= 1;
			}
			else
			{
				Cursor->StickyX -= 1;	
			}
		}		
	}
}

int E_BufferIndexFromCursor(editable_buffer *Buffer, cursor *Cursor)
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

void E_InsertTextAtCursor(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert)
{
	if(!E_Context->Temp)
	{
		E_Context->Temp = ArenaPushZero(&E_Context->Arena, MegaBytes(1));
	}

	int At = E_BufferIndexFromCursor(Buffer, Cursor);

	//TODO(Oliver): Not implemented yet
	assert(Buffer->String.Size + TextToInsert.Size < MegaBytes(1));
	assert(E_Context->Temp);
	
	MemoryCopy(E_Context->Temp, Buffer->String.Bytes, At);
	MemoryCopy(E_Context->Temp + At, TextToInsert.Bytes, TextToInsert.Size);
	MemoryCopy(E_Context->Temp + At + TextToInsert.Size, Buffer->String.Bytes + At, Buffer->String.Size - At);

	MemoryCopy(Buffer->String.Bytes, E_Context->Temp, Buffer->String.Size + TextToInsert.Size);


	E_AdvanceCursorXByN(Cursor, Buffer, TextToInsert.Size);

	Buffer->String.Size += TextToInsert.Size;
}

void E_RemoveTextFromTextRange(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd)
{
	if(!E_Context->Temp)
	{
		E_Context->Temp = ArenaPushZero(&E_Context->Arena, MegaBytes(1));
	}

	assert(E_Context->Temp);
	
	MemoryCopy(E_Context->Temp, Buffer->String.Bytes, RangeStart);
	MemoryCopy(E_Context->Temp + RangeStart, Buffer->String.Bytes + RangeEnd, Buffer->String.Size - RangeEnd);

	MemoryCopy(Buffer->String.Bytes, E_Context->Temp, Buffer->String.Size - (RangeEnd-RangeStart));

	Buffer->String.Size -= (RangeEnd-RangeStart);

	E_AdvanceCursorXByN(Cursor, Buffer, -(RangeEnd-RangeStart));
}

v2f E_GetPositionFromEditableBuffer(draw_context *D_Context,
								  cursor *Cursor,
								  editable_buffer Buffer,
								  d_font Font,
								  v2f Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer.HeightInPixels);

	f32 StartX = Position.X;
	v2f Result = Position;

	int At = E_BufferIndexFromCursor(&Buffer, Cursor);
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

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D_Context, Font, Buffer.HeightInPixels, CodePoint);

		Result.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}
	Result.Y += Font.Descent*Scale;
	return Result;
}

int E_DistanceFromNextNewLine(cursor *Cursor,
							  editable_buffer Buffer)
{
	int At = E_BufferIndexFromCursor(&Buffer, Cursor); 
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

int E_DistanceFromPreviousNewLine(cursor *Cursor,
								  editable_buffer Buffer)
{
	int At = E_BufferIndexFromCursor(&Buffer, Cursor); 
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