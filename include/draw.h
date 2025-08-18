#pragma once
#include "memory.h"
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
	v4i Box;
	u32 CodePoint;
	f32 AdvanceX;

	v2f *Curves;
	int CurveStart;
	int CurveCount;

	struct d_atlas_glyph *NextInHashSlot;

	struct d_atlas_glyph *NextInLRU;
	struct d_atlas_glyph *PreviousInLRU;
} d_atlas_glyph;

#define D_GLYPH_TABLE_SIZE (8192)

#define D_FONT_ATLAS_SIZE (4096)

#define D_MAX_CURVES (MegaBytes(16))

typedef struct draw_context
{
	m_arena *Arena;
	render_context *Render;

	v2f ViewportSize;

	d_atlas_glyph GlyphTable[D_GLYPH_TABLE_SIZE];

	d_atlas_glyph *OldestGlyph;
	d_atlas_glyph *NewestGlyph;

	
	int CurveCount;

	u64 HashOfNewestGlyph;

	int WhichAtlas;
	u8 *FontAtlas[2];
} draw_context;

d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D_Context,
									d_font Font,
									u32 CodePoint);

d_atlas_glyph *D_GlyphFromCodePoint2(draw_context *D_Context,
									u64 FontHash,
									d_font Font,
									u32 CodePoint);

void D_EvictGlyphFromTable(draw_context *D_Context, d_atlas_glyph *Glyph);


d_font D_LoadFontFromFile(draw_context *D_Context, str8 FileName);
d_font D_LoadFontFromTTF(draw_context *D_Context, u8 *Data);
void D_DrawGlyph(draw_context *D_Context, d_atlas_glyph *Glyph);
void D_Draw3DStr8(draw_context *D_Context,
				  m_arena *Arena,
				  d_font Font,
				  v3f Position,
				  v4f Colour,
				  f32 HeightInPixels,
				  str8 String);
void D_DrawStr8(draw_context *D_Context,
				m_arena *Arena,
				d_font Font,
				v2f Position,
				v4f Colour,
				f32 HeightInPixels,
				str8 String);

void D_DrawSelectionRange(draw_context *D,
						  m_arena *Arena,
						  d_font Font,
						  v2f Position,
						  v4f Colour,
						  f32 HeightInPixels,
						  str8 String,
						  v2i Range,
						  s32 Start,
						  s32 End);

void D_DrawQuad(draw_context *D_Context,
				m_arena *Arena, v2f Position,
				f32 Width, f32 Height,
				v4f Colour);

void D_DrawCubePoints(draw_context *D,
					  m_arena *Arena, v3f Position,
					  v3f Size,
					  v4f Colour);

void D_DrawPointsArray
(
	draw_context *D,
	m_arena *Arena,
	v3f *Positions,
	int PositionsCount,
	f32 PointSize,
	v4f Colour
);

void D_DrawPointsArrayWithColours
(
	draw_context *D,
	m_arena *Arena,
	v3f *Positions,
	int PositionsCount,
	f32 PointSize,
	v4f *Colours
);

void D_DrawLinesArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  f32 Thickness,
					  v4f Colour);

void D_DrawLine(draw_context *D,
				m_arena *Arena,
				v3f Start,
				v3f End,
				f32 Thickness,
				v4f Colour);

void D_DrawBezierArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  f32 Thickness,
					  v4f Colour);

void D_DrawBezier(draw_context *D,
				  m_arena *Arena,
				  v3f Start,
				  v3f Control,
				  v3f End,
				  v4f Colour);

void D_DrawGrid(draw_context *Draw, m_arena *Arena, f32 Size, s32 Count);

void D_DrawGrid2D(draw_context *Draw, m_arena *Arena, f32 Size, s32 Count);

f32 D_LineHeightFromFont(d_font Font, f32 HeightInPixels);
