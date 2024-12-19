#include "edit.h"

#include "memory.h"
#include "misc.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>

void E_InsertTextAtCursor(edit_context *E_Context, cursor Cursor, editable_buffer *Buffer, str8 TextToInsert)
{
	if(!E_Context->Temp)
	{
		E_Context->Temp = ArenaPushZero(&E_Context->Arena, MegaBytes(1));
	}
	//TODO(Oliver): Not implemented yet
	assert(Buffer->String.Size + TextToInsert.Size < MegaBytes(1));
	assert(E_Context->Temp);
	
	MemoryCopy(E_Context->Temp, Buffer->String.Bytes, Cursor.At);
	MemoryCopy(E_Context->Temp + Cursor.At, TextToInsert.Bytes, TextToInsert.Size);
	MemoryCopy(E_Context->Temp + Cursor.At + TextToInsert.Size, Buffer->String.Bytes + Cursor.At, Buffer->String.Size - Cursor.At);

	MemoryCopy(Buffer->String.Bytes, E_Context->Temp, Buffer->String.Size + TextToInsert.Size);
}

void E_RemoveTextFromTextRange(edit_context *E_Context, cursor Cursor, editable_buffer *Buffer,
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
	for(int I = 0; I < Buffer.String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = Buffer.String.Bytes[I];

		if(I == Cursor->At)
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
	int I = Cursor->At;
	for(;I < Buffer.String.Size; I++)
	{
		if(Buffer.String.Bytes[I] == '\n')
		{
			break;
		}
	}
	return I - Cursor->At;
}

int E_DistanceFromPreviousNewLine(cursor *Cursor,
								  editable_buffer Buffer)
{
	int I = Cursor->At;
	for(;I >= 0; I--)
	{
		if(Buffer.String.Bytes[I] == '\n')
		{
			break;
		}
	}
	return Cursor->At - I;
}

void InsertTextAtCursor(memory_arena *Arena, cursor *Cursor, editable_buffer Buffer, str8 TextToInsert)
{

}