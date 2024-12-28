#include "log.h"
#include "maths.h"
#include "memory.h"
#include "render.h"
#include "strings.h"
#include "draw.h"

#include <assert.h>
#include <math.h>

d_font D_LoadFontFromTTF(draw_context *D_Context, u8 *Data)
{
	d_font Font = {0};
	stbtt_fontinfo Info = {0};
	int Status = stbtt_InitFont(&Info, Data, 0);
	if(!Status)
	{
		LogError("Internal font file is invalid");
	}
	else
	{
		stbtt_GetFontVMetrics(&Info, &Font.Ascent, &Font.Descent, &Font.LineGap);
		Font.Info = Info;
	}
	return Font;
}

void D_GlyphsToAtlas(draw_context *D)
{
	if(!D->NewestGlyph)
	{
		//LogInfo("No glyphs to place in atlas");
		return;
	}

	if(!D->FontAtlas[D->WhichAtlas % 2])
	{
		D->FontAtlas[D->WhichAtlas % 2] = ArenaPushZero(D->Arena, D_FONT_ATLAS_SIZE*D_FONT_ATLAS_SIZE);
	}

	u64 HashOfNewestGlyph = M_FNVHashBytes(M_FNV_HASH_START,
												(u8*)D->NewestGlyph,
												sizeof(d_atlas_glyph));

	if(HashOfNewestGlyph != D->HashOfNewestGlyph)
	{
		MemorySet(D->FontAtlas[D->WhichAtlas % 2],  0, D_FONT_ATLAS_SIZE*D_FONT_ATLAS_SIZE);
		LogInfo("Atlas will be updated");
		D->HashOfNewestGlyph = HashOfNewestGlyph;

		int TotalY = D_FONT_ATLAS_SIZE;
		int AtlasY = 0;
		int AtlasX = 0;
		int TallestYOnThisRow = 0;

		int Padding = 8;
		for(d_atlas_glyph *Glyph = D->NewestGlyph;
			Glyph != Null;
			Glyph = Glyph->PreviousInLRU)
		{
			int Width = Glyph->BitmapBox.Z - Glyph->BitmapBox.X;
			int Height = Glyph->BitmapBox.W - Glyph->BitmapBox.Y;
			TallestYOnThisRow = Max(TallestYOnThisRow, Height + Padding);

			if(AtlasX + Width + Padding > D_FONT_ATLAS_SIZE)
			{
				AtlasY += TallestYOnThisRow;
				AtlasX = 0;
				TallestYOnThisRow = 0;
			}
			else if(AtlasY + Height + Padding > D_FONT_ATLAS_SIZE)
			{
				// No more atlas space left, this is where we could just
				// allocate a bigger texture . For now we'll just evict
				// the remaining glyphs. Since we are iterating
				// newest to oldest, the current glyph is the 
				// oldest invalid glyph, so we pop all glyphs
				// from this glyph onwards.
				D->OldestGlyph = Glyph->NextInLRU;
				D->OldestGlyph->PreviousInLRU = Null;
				LogInfo("Evicted Glyphs");
			}
			else
			{
				Glyph->AtlasPosition = v2f(AtlasX, AtlasY);
				u8 *Row = D->FontAtlas[D->WhichAtlas % 2] + AtlasX + AtlasY * D_FONT_ATLAS_SIZE;
				for(int Y = 0; Y < Height; Y++)
				{
					for(int X = 0; X < Width; X++)
					{
						Row[X] = Glyph->Data[X + Y*Width];
					}
					Row += D_FONT_ATLAS_SIZE;
				}
			}
			AtlasX += Width + Padding;
		}
		R_TextureFromBytes(D->Render, D->FontAtlas[D->WhichAtlas++ % 2], D_FONT_ATLAS_SIZE, D_FONT_ATLAS_SIZE);
	}
}

void PushGlyphOntoLRU(draw_context *D_Context, d_atlas_glyph *Glyph)
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

void PopGlyphOffLRU(draw_context *D_Context)
{
	if(D_Context->OldestGlyph)
	{
		D_Context->OldestGlyph = D_Context->OldestGlyph->NextInLRU;
		D_Context->OldestGlyph->PreviousInLRU = Null;
	}
}

d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D, d_font Font, s32 HeightInPixels, u32 CodePoint)
{
	per_thread_context *ThreadContext = GetThreadContext();
	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&HeightInPixels, sizeof(s32));
	Hash = M_FNVHashBytes(Hash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	for(;Glyph->NextInHashSlot != Null && Glyph->Hash != Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *ResultGlyph = Glyph;

	float Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	
	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		LogInfo("Glyph collision occured");
		d_atlas_glyph *NextGlyph = ArenaPushTypeZero(D->Arena, d_atlas_glyph);
		Glyph->NextInHashSlot = NextGlyph;

		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);
		NextGlyph->AdvanceX = AdvanceX;
		NextGlyph->Hash = Hash;
		NextGlyph->CodePoint = CodePoint;
		stbtt_GetCodepointBox(&Font.Info, CodePoint, &NextGlyph->Box.X, &NextGlyph->Box.Y, 
													 &NextGlyph->Box.Z, &NextGlyph->Box.W);

		PushGlyphOntoLRU(D, NextGlyph);

		v4i BitmapBox = {0};
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&BitmapBox.X, &BitmapBox.Y,
									&BitmapBox.Z, &BitmapBox.W);
		
		int GlyphSizeInBytes = (BitmapBox.Z - BitmapBox.X)*(BitmapBox.W - BitmapBox.Y);
		NextGlyph->Data = ArenaPushZero(D->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  NextGlyph->Data,
								  BitmapBox.Z - BitmapBox.X,
								  BitmapBox.W - BitmapBox.Y,
								  BitmapBox.Z - BitmapBox.X,
								  Scale, Scale,
								  NextGlyph->CodePoint);
		NextGlyph->BitmapBox = BitmapBox;
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);
		
		Glyph->AdvanceX = AdvanceX;
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		stbtt_GetCodepointBox(&Font.Info, Glyph->CodePoint, &Glyph->Box.X, &Glyph->Box.Y,
															&Glyph->Box.Z, &Glyph->Box.W);
		PushGlyphOntoLRU(D, Glyph);


		v4i BitmapBox = {0};
		stbtt_GetCodepointBitmapBox(&Font.Info,
									CodePoint,
									Scale, Scale,
									&BitmapBox.X, &BitmapBox.Y,
									&BitmapBox.Z, &BitmapBox.W);
		int GlyphSizeInBytes = (BitmapBox.Z - BitmapBox.X)*(BitmapBox.W - BitmapBox.Y);
		Glyph->Data = ArenaPushZero(D->Arena, GlyphSizeInBytes);
		stbtt_MakeCodepointBitmap(&Font.Info,
								  Glyph->Data,
								  BitmapBox.Z - BitmapBox.X,
								  BitmapBox.W - BitmapBox.Y,
								  BitmapBox.Z - BitmapBox.X,
								  Scale, Scale,
								  Glyph->CodePoint);
		Glyph->BitmapBox = BitmapBox;
	}

	assert(CodePoint == ResultGlyph->CodePoint);

	return ResultGlyph;
}

void D_DrawQuad(draw_context *D,
				m_arena *Arena, v2f Position,
				f32 Width, f32 Height,
				v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, 6*sizeof(r_vertex));
	Command.VerticesCount = 6;
	Command.VerticesFormat = R_VF_POSITION | R_VF_UV;

	r_vertex *R_Vertices = Command.Vertices;
	
	v2f Vertices[6] = 
	{
		v2f(Position.X, Position.Y),
		v2f(Position.X + Width, Position.Y),
		v2f(Position.X + Width, Position.Y + Height),

		v2f(Position.X, Position.Y),
		v2f(Position.X, Position.Y + Height),
		v2f(Position.X + Width, Position.Y + Height),
	};

	for(int V = 0; V < 6; V++)
	{
		r_vertex Vertex = {
			.Position = v4f(Vertices[V].X, Vertices[V].Y, 0, 1),
			.Colour = Colour,
		};

		*R_Vertices++ = Vertex;
	}
	Command.Type = R_COMMAND_TRIANGLE;
	R_PushCommand(R, Arena, Command);
}

void D_DrawStr8(draw_context *D,
				m_arena *Arena,
				d_font Font,
				v2f Position,
				v4f Colour,
				f32 HeightInPixels,
				str8 String)
{
	render_context *R = D->Render;

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	int TotalTriangleCount = String.Size*6;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, TotalTriangleCount*sizeof(r_vertex));
	Command.VerticesCount = TotalTriangleCount;
	Command.VerticesFormat = R_VF_POSITION | R_VF_UV;

	f32 StartX = Position.X;
	r_vertex *R_Vertices = Command.Vertices;

	b32 NewLine = False;
	v4f Colours[2] = {Colour, v4f(0, 0, 1, 1) };
	for(int I = 0; I < String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = String.Bytes[I];



		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}

		if(CodePoint == '\n')
		{
			NewLine = True;
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			continue;
		}
		else
		{
			NewLine = False;
		}
		
		//if(NewLine) CodePoint = 'N';

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D,  Font, HeightInPixels, CodePoint);

		f32 GlyphWidth = ((f32)(Glyph->Box.Z - Glyph->Box.X)*Scale);
		f32 GlyphHeight = ((f32)(Glyph->Box.W - Glyph->Box.Y)*Scale);

		f32 GlyphBitmapWidth = ((f32)(Glyph->BitmapBox.Z - Glyph->BitmapBox.X));
		f32 GlyphBitmapHeight = ((f32)(Glyph->BitmapBox.W - Glyph->BitmapBox.Y));

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
			
			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y),

			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y)
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y, 0, 1),
				.UV = DivV2F(UVs[V], V2FromF32((f32)D_FONT_ATLAS_SIZE)),
				.Colour = Colours[NewLine],
			};

			Vertex.Position.Y += (Glyph->Box.Y*Scale);
			Vertex.Position.X += (Glyph->Box.X*Scale);

			if(CodePoint != ' ') *R_Vertices++ = Vertex;
		}

		if(NewLine)
		{
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			continue;
		}
		Position.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}

	Command.Type = R_COMMAND_GLYPH;
	R_PushCommand(R, Arena, Command);
}

void D_DrawSelectionRange(draw_context *D,
						  m_arena *Arena,
						  d_font Font,
						  v2f Position,
						  v4f Colour,
						  f32 HeightInPixels,
						  str8 String,
						  s32 Start,
						  s32 End)
{
	if(Start == End) return;

	render_context *R = D->Render;

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);

	f32 StartX = Position.X;

	f32 SelectionStartX = StartX;

	b32 InRange = False;

	for(int I = 0; I < String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = String.Bytes[I];

		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}


		if(I == Start) SelectionStartX = Position.X;
		if((I == End) || (I >= Start && I <= End && CodePoint == '\n'))
		{
			if(CodePoint == '\n')
			{
				// NOTE(Oliver): I only do this so that you can see if you're selecting
				// whitespace
				d_atlas_glyph *Space  = D_GlyphFromCodePoint(D, Font, HeightInPixels, ' ');
				Position.X += Space->AdvanceX * Scale * TabMultiplier;
			}
			D_DrawQuad(D, Arena, v2f(SelectionStartX, Position.Y + Font.Descent*Scale), Position.X - SelectionStartX, (Font.Ascent - Font.Descent + Font.LineGap)*Scale, Colour);
		}

		if(CodePoint == '\n')
		{
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			SelectionStartX = StartX;
			continue;
		}


		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, HeightInPixels, CodePoint);		

		Position.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}
}

f32 D_LineHeightFromFont(d_font Font, f32 HeightInPixels)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);

	return (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
}

void D_DrawCubePoints(draw_context *D,
					  m_arena *Arena, v3f Position,
					  v3f Size,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, 8*sizeof(r_vertex));
	Command.VerticesCount = 8;
	Command.VerticesFormat = R_VF_POSITION | R_VF_COLOUR;

	r_vertex *R_Vertices = Command.Vertices;
	
	f32 Width = Size.X;
	f32 Height = Size.Y;
	f32 Depth = Size.Z;

	float HalfWidth = Width  * 0.5f;
	float HalfHeight = Height * 0.5f;
	float HalfDepth = Depth  * 0.5f;

	v3f Vertices[8] =
	{
		v3f(Position.X - HalfWidth, Position.Y - HalfHeight, Position.Z - HalfDepth),
		v3f(Position.X + HalfWidth, Position.Y - HalfHeight, Position.Z - HalfDepth),
		v3f(Position.X + HalfWidth, Position.Y + HalfHeight, Position.Z - HalfDepth),
		v3f(Position.X - HalfWidth, Position.Y + HalfHeight, Position.Z - HalfDepth),

		v3f(Position.X - HalfWidth, Position.Y - HalfHeight, Position.Z + HalfDepth),
		v3f(Position.X + HalfWidth, Position.Y - HalfHeight, Position.Z + HalfDepth),
		v3f(Position.X + HalfWidth, Position.Y + HalfHeight, Position.Z + HalfDepth),
		v3f(Position.X - HalfWidth, Position.Y + HalfHeight, Position.Z + HalfDepth)
	};

	for(int V = 0; V < 8; V++)
	{
		r_vertex Vertex = {
			.Position = v4f(Vertices[V].X, Vertices[V].Y, Vertices[V].Z, 1),
			.Colour = Colour,
		};

		*R_Vertices++ = Vertex;
	}
	Command.Type = R_COMMAND_POINTS;
	R_PushCommand(R, Arena, Command);
}

void D_DrawPointsArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, PositionsCount*sizeof(r_vertex));
	Command.VerticesCount = PositionsCount;
	Command.VerticesFormat = R_VF_POSITION | R_VF_COLOUR;

	r_vertex *R_Vertices = Command.Vertices;

	for(int V = 0; V < PositionsCount; V++)
	{
		r_vertex Vertex = {
			.Position = v4f(Positions[V].X, Positions[V].Y, Positions[V].Z, 1),
			.Colour = Colour,
		};

		*R_Vertices++ = Vertex;
	}
	Command.Type = R_COMMAND_POINTS;
	R_PushCommand(R, Arena, Command);
}


void D_DrawLinesArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, PositionsCount*sizeof(r_vertex));
	Command.VerticesCount = PositionsCount;
	Command.VerticesFormat = R_VF_POSITION | R_VF_COLOUR;

	r_vertex *R_Vertices = Command.Vertices;

	for(int V = 0; V < PositionsCount; V++)
	{
		r_vertex Vertex = {
			.Position = v4f(Positions[V].X, Positions[V].Y, Positions[V].Z, 1),
			.Colour = Colour,
		};

		*R_Vertices++ = Vertex;
	}
	Command.Type = R_COMMAND_LINES;
	R_PushCommand(R, Arena, Command);
}


void D_DrawGrid(draw_context *Draw, m_arena *Arena, f32 Size, s32 Count)
{
	s32 LineCount = (Count + 1) * 2; 
	v3f *Lines = (v3f*)ArenaPushZero(Arena, LineCount * 2 * sizeof(v3f)); // Each line has 2 vertices

	int Index = 0;

	f32 HalfSize = (Count * Size) / 2.0f;

	for (s32 I = 0; I <= Count; I++)
	{
		f32 Z = -HalfSize + (I * Size);
		Lines[Index++] = v3f(-HalfSize, -1, Z);
		Lines[Index++] = v3f(HalfSize,  -1, Z);
	}

	for (s32 I = 0; I <= Count; I++)
	{
		f32 X = -HalfSize + (I * Size);
		Lines[Index++] = v3f(X, -1, -HalfSize);
		Lines[Index++] = v3f(X, -1, HalfSize);
	}

	D_DrawLinesArray(Draw, Arena, Lines, LineCount * 2, v4f(1.0, .05, 0, 1));
}