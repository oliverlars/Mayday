#pragma once
#include "render.h"
#include "stb_truetype.h"

typedef struct d_font
{
	stbtt_fontinfo Info;
	int Ascent;
	int Descent;
	int LineGap;
} d_font;

typedef struct d_atlas_glyph
{
	u64 Hash;
	u8 *Data;
	v2f AtlasPosition;
	v4i Box;
	v4i BitmapBox;
	u32 CodePoint;
	f32 AdvanceX;
	struct d_atlas_glyph *NextInHashSlot;

	struct d_atlas_glyph *NextInLRU;
	struct d_atlas_glyph *PreviousInLRU;
} d_atlas_glyph;

#define D_GLYPH_TABLE_SIZE (4096)

#define D_FONT_ATLAS_SIZE (4096)

typedef struct draw_context
{
	memory_arena *Arena;
	logger *Logger;

	render_context *R;

	d_atlas_glyph GlyphTable[D_GLYPH_TABLE_SIZE];

	d_atlas_glyph *OldestGlyph;
	d_atlas_glyph *NewestGlyph;

	u64 HashOfNewestGlyph;

	u8 *FontAtlas;
} draw_context;

d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D_Context,
									d_font Font,
									s32 Scale,
									u32 CodePoint);

void D_EvictGlyphFromTable(draw_context *D_Context, d_atlas_glyph *Glyph);

void D_GlyphsToAtlas(draw_context *D_Context);


d_font D_LoadFontFromFile(draw_context *D_Context, str8 FileName);
d_font D_LoadFontFromTTF(draw_context *D_Context, u8 *Data);
void D_DrawGlyph(draw_context *D_Context, d_atlas_glyph *Glyph);
void D_DrawStr8(draw_context *D_Context,
				memory_arena *Arena,
				d_font Font,
				v2f Position,
				v4f Colour,
				f32 HeightInPixels,
				str8 String);

void D_DrawQuad(draw_context *D_Context,
				memory_arena *Arena, v2f Position,
				f32 Width, f32 Height,
				v4f Colour);

f32 D_LineHeightFromFont(d_font Font, f32 HeightInPixels);