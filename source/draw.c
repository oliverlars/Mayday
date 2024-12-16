#include "log.h"
#include "maths.h"
#include "memory.h"
#include "render.h"
#include "draw.h"

d_font D_LoadFontFromTTF(d_context *D_Context, u8 *Data)
{
	d_font Font = {0};
	int Status = stbtt_InitFont(&Font.Info, Data, 0);
	if(!Status)
	{
		LogError(D_Context->Logger, "Internal font file is invalid");
	}
	return Font;
}

void D_GlyphsToAtlas(d_context *D_Context)
{
	if(!D_Context->NewestGlyph)
	{
		LogInfo(D_Context->Logger, "No glyphs to place in atlas");
		return;
	}

	u64 HashOfNewestGlyph = M_FNVHashBytes(M_FNV_HASH_START,
												(u8*)D_Context->NewestGlyph,
												sizeof(d_atlas_glyph));

	if(HashOfNewestGlyph != D_Context->HashOfNewestGlyph)
	{
		LogInfo(D_Context->Logger, "Atlas will be updated");
		D_Context->HashOfNewestGlyph = HashOfNewestGlyph;
	}
}

void PushGlyphOntoLRU(d_context *D_Context, d_atlas_glyph *Glyph)
{
	if(!D_Context->OldestGlyph)
	{
		D_Context->OldestGlyph = Glyph;
		D_Context->NewestGlyph = Glyph;
	}
	else
	{
		Glyph->PreviousInLRU = D_Context->NewestGlyph;
		D_Context->NewestGlyph->NextInLRU = Glyph;
		D_Context->NewestGlyph = Glyph;
	}
}

void PopGlyphOffLRU(d_context *D_Context)
{
	if(D_Context->OldestGlyph)
	{
		D_Context->OldestGlyph = D_Context->OldestGlyph->NextInLRU;
		D_Context->OldestGlyph->PreviousInLRU = Null;
	}
}

d_atlas_glyph *D_GlyphFromCodePoint(d_context *D_Context, d_font Font, s32 HeightInPixels, u32 CodePoint)
{
	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&HeightInPixels, sizeof(s32));
	Hash = M_FNVHashBytes(Hash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D_Context->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	for(;Glyph->NextInHashSlot != Null && Glyph->Hash == Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *ResultGlyph = Glyph;

	int Ascent;
	int Descent;
	int LineGap;
	stbtt_GetFontVMetrics(&Font.Info, &Ascent, &Descent, &LineGap);
	float Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	
	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		LogInfo(D_Context->Logger, "Glyph collision occured");
		d_atlas_glyph *NextGlyph = ArenaPushTypeZero(D_Context->Arena, d_atlas_glyph);
		Glyph->NextInHashSlot = NextGlyph;
		NextGlyph->Hash = Hash;
		NextGlyph->CodePoint = CodePoint;

		PushGlyphOntoLRU(D_Context, NextGlyph);
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&NextGlyph->Box.X, &NextGlyph->Box.Y,
									&NextGlyph->Box.Z, &NextGlyph->Box.W);
		int GlyphSizeInBytes = sizeof(u8)*(Glyph->Box.Z - Glyph->Box.X)*(Glyph->Box.W - Glyph->Box.Y);
		Glyph->Data = ArenaPushZero(D_Context->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  Glyph->Data,
								  Glyph->Box.Z - Glyph->Box.X,
								  Glyph->Box.W - Glyph->Box.Y,
								  1,
								  Scale, Scale,
								  Glyph->CodePoint);
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		LogInfo(D_Context->Logger, "Empty glyph hash slot is now filled");
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		PushGlyphOntoLRU(D_Context, Glyph);
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&Glyph->Box.X, &Glyph->Box.Y,
									&Glyph->Box.Z, &Glyph->Box.W);
		int GlyphSizeInBytes = sizeof(u8)*(Glyph->Box.Z - Glyph->Box.X)*(Glyph->Box.W - Glyph->Box.Y);
		Glyph->Data = ArenaPushZero(D_Context->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  Glyph->Data,
								  Glyph->Box.Z - Glyph->Box.X,
								  Glyph->Box.W - Glyph->Box.Y,
								  1,
								  Scale, Scale,
								  Glyph->CodePoint);
	}
	else
	{
		LogInfo(D_Context->Logger, "Glyph already in table");
	}
	return ResultGlyph;
}

void D_DrawStr8(r_context *R_Context, d_font Font, int LineHeight, str8 String)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, LineHeight);

	int Ascent;
	int Descent;
	int LineGap;
	stbtt_GetFontVMetrics(&Font.Info, &Ascent, &Descent, &LineGap);

	for(int I = 0; I < String.Size; I++)
	{
		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, String.Bytes[I], &AdvanceX, &LeftSideBearing);

		v4i Box;
		stbtt_GetCodepointBitmapBox(&Font.Info, String.Bytes[I], Scale, Scale, &Box.X, &Box.Y, &Box.Z, &Box.W);
		int Y = Ascent + Box.W;

		//int byteOffset = x + roundf(lsb * scale) + (y * b_w);
		//stbtt_MakeCodepointBitmap(&info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, b_w, scale, scale, word[i]);

	}
}