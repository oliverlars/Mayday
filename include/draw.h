#pragma once
#include "render.h"
#include "stb_truetype.h"

typedef struct d_font
{
	stbtt_fontinfo Info;
} d_font;

typedef struct d_atlas_glyph
{
	u64 Hash;
	u8 *Data;
	v4i Box;
	u32 CodePoint;
	struct d_atlas_glyph *NextInHashSlot;

	struct d_atlas_glyph *NextInLRU;
	struct d_atlas_glyph *PreviousInLRU;
} d_atlas_glyph;

#define D_GLYPH_TABLE_SIZE (1)

typedef struct d_context
{
	memory_arena *Arena;
	logger *Logger;
	d_atlas_glyph GlyphTable[D_GLYPH_TABLE_SIZE];

	d_atlas_glyph *OldestGlyph;
	d_atlas_glyph *NewestGlyph;

	u64 HashOfNewestGlyph;

	u8 *FontAtlas;
	s32 AtlasSize;
} d_context;

d_atlas_glyph *D_GlyphFromCodePoint(d_context *D_Context,
									d_font Font,
									s32 Scale,
									u32 CodePoint);

void D_EvictGlyphFromTable(d_context *D_Context, d_atlas_glyph *Glyph);

void D_GlyphsToAtlas(d_context *D_Context);


d_font D_LoadFontFromFile(d_context *D_Context, str8 FileName);
d_font D_LoadFontFromTTF(d_context *D_Context, u8 *Data);
void D_DrawStr8(r_context *R_Context, d_font Font, int LineHeight, str8 String);