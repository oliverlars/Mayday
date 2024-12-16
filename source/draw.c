#include "log.h"
#include "maths.h"
#include "memory.h"
#include "render.h"
#include "strings.h"
#include "draw.h"

#include <assert.h>

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

	if(!D_Context->FontAtlas)
	{
		D_Context->FontAtlas = ArenaPushZero(D_Context->Arena, D_FONT_ATLAS_SIZE*D_FONT_ATLAS_SIZE);
	}

	u64 HashOfNewestGlyph = M_FNVHashBytes(M_FNV_HASH_START,
												(u8*)D_Context->NewestGlyph,
												sizeof(d_atlas_glyph));

	if(HashOfNewestGlyph != D_Context->HashOfNewestGlyph)
	{
		LogInfo(D_Context->Logger, "Atlas will be updated");
		D_Context->HashOfNewestGlyph = HashOfNewestGlyph;

		int TotalY = D_FONT_ATLAS_SIZE;
		int AtlasY = 0;
		int AtlasX = 0;
		int TallestYOnThisRow = 0;
		for(d_atlas_glyph *Glyph = D_Context->NewestGlyph;
			Glyph != Null;
			Glyph = Glyph->PreviousInLRU)
		{
			int Width = Glyph->Box.Z - Glyph->Box.X;
			int Height = Glyph->Box.W - Glyph->Box.Y;
			TallestYOnThisRow = Max(TallestYOnThisRow, Height);

			if(AtlasX + Width > D_FONT_ATLAS_SIZE)
			{
				AtlasY += TallestYOnThisRow;
				AtlasX = 0;
				TallestYOnThisRow = 0;
			}
			else if(AtlasY + Height > D_FONT_ATLAS_SIZE)
			{
				// No more atlas space left, this is where we could just
				// allocate a bigger texture . For now we'll just evict
				// the remaining glyphs. Since we are iterating
				// newest to oldest, the current glyph is the 
				// oldest invalid glyph, so we pop all glyphs
				// from this glyph onwards.
				D_Context->OldestGlyph = Glyph->NextInLRU;
				D_Context->OldestGlyph->PreviousInLRU = Null;
			}
			else
			{
				Glyph->AtlasPosition = v2f(AtlasX, AtlasY);
				u8 *Row = D_Context->FontAtlas + AtlasX + AtlasY * D_FONT_ATLAS_SIZE;
				for(int Y = 0; Y < Height; Y++)
				{
					for(int X = 0; X < Width; X++)
					{
						Row[X] = Glyph->Data[X + Y*Width];
					}
					Row += D_FONT_ATLAS_SIZE;
				}
			}
			AtlasX += Width;
		}
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
	for(;Glyph->NextInHashSlot != Null && Glyph->Hash != Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *ResultGlyph = Glyph;

	float Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	
	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		LogInfo(D_Context->Logger, "Glyph collision occured");
		d_atlas_glyph *NextGlyph = ArenaPushTypeZero(D_Context->Arena, d_atlas_glyph);
		Glyph->NextInHashSlot = NextGlyph;

		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);
		Glyph->AdvanceX = AdvanceX;
		NextGlyph->Hash = Hash;
		NextGlyph->CodePoint = CodePoint;

		PushGlyphOntoLRU(D_Context, NextGlyph);
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&NextGlyph->Box.X, &NextGlyph->Box.Y,
									&NextGlyph->Box.Z, &NextGlyph->Box.W);
		int GlyphSizeInBytes = (NextGlyph->Box.Z - NextGlyph->Box.X)*(NextGlyph->Box.W - NextGlyph->Box.Y);
		NextGlyph->Data = ArenaPushZero(D_Context->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  NextGlyph->Data,
								  NextGlyph->Box.Z - NextGlyph->Box.X,
								  NextGlyph->Box.W - NextGlyph->Box.Y,
								  Glyph->Box.Z - Glyph->Box.X,
								  Scale, Scale,
								  NextGlyph->CodePoint);
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		LogInfo(D_Context->Logger, "Empty glyph hash slot is now filled");

		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);

		Glyph->AdvanceX = AdvanceX;
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		PushGlyphOntoLRU(D_Context, Glyph);
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&Glyph->Box.X, &Glyph->Box.Y,
									&Glyph->Box.Z, &Glyph->Box.W);
		int GlyphSizeInBytes = (Glyph->Box.Z - Glyph->Box.X)*(Glyph->Box.W - Glyph->Box.Y);
		Glyph->Data = ArenaPushZero(D_Context->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  Glyph->Data,
								  Glyph->Box.Z - Glyph->Box.X,
								  Glyph->Box.W - Glyph->Box.Y,
								  Glyph->Box.Z - Glyph->Box.X,
								  Scale, Scale,
								  Glyph->CodePoint);
	}
	else
	{
		LogInfo(D_Context->Logger, "Glyph already in table");
	}
	return ResultGlyph;
}

void D_DrawGlyph(r_context *R_Context, d_atlas_glyph *Glyph)
{

}

void D_DrawStr8(r_context *R_Context,
				d_context *D_Context,
				memory_arena *Arena,
				v2f Position,
				d_font Font,
				f32 HeightInPixels,
				str8 String)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	int TotalTriangleCount = String.Size*6;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPush(Arena, TotalTriangleCount*sizeof(r_vertex));
	Command.VerticesCount = TotalTriangleCount;
	Command.VerticesFormat = R_VF_POSITION | R_VF_UV;



	r_vertex *R_Vertices = Command.Vertices;
	for(int I = 0; I < String.Size; I++)
	{

		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = String.Bytes[I];
		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D_Context, Font, HeightInPixels, CodePoint);

		f32 GlyphWidth = (f32)(Glyph->Box.Z - Glyph->Box.X);
		f32 GlyphHeight = (f32)(Glyph->Box.W - Glyph->Box.Y);
		//LogInfo(D_Context->Logger, "?f32 ?f32", GlyphWidth, GlyphHeight);

		v2f Vertices[6] = 
		{
			v2f(Position.X, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),

			v2f(Position.X, Position.Y),
			v2f(Position.X, Position.Y + GlyphHeight),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),
		};

		v2f UVs[6] = 
		{
			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y),
			v2f(Glyph->AtlasPosition.X + GlyphWidth, Glyph->AtlasPosition.Y),
			v2f(Glyph->AtlasPosition.X + GlyphWidth, Glyph->AtlasPosition.Y + GlyphHeight),

			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y),
			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphHeight),
			v2f(Glyph->AtlasPosition.X + GlyphWidth, Glyph->AtlasPosition.Y + GlyphHeight),
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y, 0, 1),
				.UV = DivV2F(UVs[V], V2FromF32((f32)D_FONT_ATLAS_SIZE)),
			};
			Vertex.Position.X /= 1280.0f;
			Vertex.Position.Y /= 720.0f;
			Vertex.Position.X = (Vertex.Position.X)*2.0 - 1.0f;
			Vertex.Position.Y = (Vertex.Position.Y)*2.0 - 1.0f;
			*R_Vertices++ = Vertex;
		}

		Position.X += Glyph->AdvanceX * Scale;
		//int byteOffset = x + roundf(lsb * scale) + (y * b_w);
		//stbtt_MakeCodepointBitmap(&info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, b_w, scale, scale, word[i]);
	}

	R_PushCommand(R_Context, Command);
}