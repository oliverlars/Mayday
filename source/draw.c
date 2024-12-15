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
		LogError(&D_Context->Logger, "Internal font file is invalid");
	}
	return Font;
}

d_atlas_glyph *D_GlyphFromCodePoint(d_context *D_Context, d_font Font, s32 Scale, u32 CodePoint)
{
	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&Scale, sizeof(s32));
	Hash = M_FNVHashBytes(Hash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D_Context->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	d_atlas_glyph *ResultGlyph = Glyph;
	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		d_atlas_glyph *NextGlyph = ArenaPushTypeZero(D_Context->Arena, d_atlas_glyph);
		Glyph->HashNext = NextGlyph;
		NextGlyph->Hash = Hash;
		NextGlyph->CodePoint = CodePoint;
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&NextGlyph->Box.X, &NextGlyph->Box.Y,
									&NextGlyph->Box.Z, &NextGlyph->Box.W);
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		LogInfo(&D_Context->Logger, "Empty glyph hash slot is now filled");
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&Glyph->Box.X, &Glyph->Box.Y,
									&Glyph->Box.Z, &Glyph->Box.W);
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