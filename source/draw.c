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
		D_Context->OldestGlyph[0] = (d_atlas_glyph){0};
		D_Context->OldestGlyph = D_Context->OldestGlyph->NextInLRU;
		D_Context->OldestGlyph->PreviousInLRU = Null;
	}
}

void CurvesFromCodePoint(draw_context *Draw, m_arena *Arena, d_atlas_glyph *Glyph, u32 CodePoint, d_font Font, int *CurveCount, v2f **Curves)
{
	assert(CurveCount != 0);
	assert(Curves != Null);
	s32 GlyphIndex = stbtt_FindGlyphIndex(&Font.Info, CodePoint);
	stbtt_vertex *Vertices;
	s32 NumberOfVertices = stbtt_GetGlyphShape(&Font.Info, GlyphIndex, &Vertices);


	v4i Box = {0};
	stbtt_GetGlyphBox(&Font.Info, GlyphIndex, &Box.X, &Box.Y, &Box.Z, &Box.W);
	f32 Width = (f32)(Box.Z - Box.X);
	f32 Height = (f32)(Box.W - Box.Y);
	v2f Size = v2f(Width, Height);

	assert(Height >= 0.0);

	v2f Shift = v2f(Box.X, Box.Y);

	// This is probably more than enough memory
	Curves[0] = ArenaPushArrayZero(Arena, v2f, NumberOfVertices*3);
	CurveCount[0] = 0;

	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	v2f First = {0};
	for (s32 I = 0; I < NumberOfVertices; I++)
	{
		stbtt_vertex *V = Vertices + I;
		switch(V->type)
		{
		case STBTT_vmove:
		{
			First = Sub(v2f(V->x, V->y), Shift);
		}
		break;
		case STBTT_vcurve:
		{
			v2f Control = Sub(v2f(V->cx, V->cy), Shift);
			v2f End = Sub(v2f(V->x, V->y), Shift);
			Curves[0][CurveCount[0]++] = v2f(End.X, End.Y);
			Curves[0][CurveCount[0]++] = v2f(Control.X, Control.Y);
			Curves[0][CurveCount[0]++] = v2f(First.X, First.Y);
			First = End;
		}
		break;
		case STBTT_vline:
		{
			v2f End = Sub(v2f(V->x, V->y), Shift);
			v2f Control = v2f((First.X + End.X)*0.5, (First.Y + End.Y)*0.5);
			Curves[0][CurveCount[0]++] = v2f(End.X, End.Y);
			Curves[0][CurveCount[0]++] = v2f(Control.X, Control.Y);
			Curves[0][CurveCount[0]++] = v2f(First.X, First.Y);
			First = End;
		}
		break;
		}
	}

	for(int I = 0; I < CurveCount[0]; I++)
	{
		Curves[0][I].X /= Size.X;
		//assert(Curves[0][I].Y >= 0);
		Curves[0][I].Y = (Height - Curves[0][I].Y) / Size.Y;
	}

	
	R_UpdateCurves(Draw->Render, Curves[0], CurveCount[0], Draw->CurveCount);
	Draw->CurveCount += CurveCount[0];
	
	ScratchEnd(Scratch);
}


d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D, d_font Font, u32 CodePoint)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	for(;Glyph->NextInHashSlot != Null && Glyph->Hash != Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *ResultGlyph = Glyph;

	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		d_atlas_glyph *NextGlyph = ArenaPushArrayZero(D->Arena, d_atlas_glyph, 1);
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

		NextGlyph->CurveStart = D->CurveCount;
		CurvesFromCodePoint(D, D->Arena, NextGlyph, CodePoint, Font, &Glyph->CurveCount, &Glyph->Curves);
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		Glyph[0] = (d_atlas_glyph){0};
		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);

		Glyph->AdvanceX = AdvanceX;
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		stbtt_GetCodepointBox(&Font.Info, Glyph->CodePoint, &Glyph->Box.X, &Glyph->Box.Y,
															&Glyph->Box.Z, &Glyph->Box.W);
		PushGlyphOntoLRU(D, Glyph);

		Glyph->CurveStart = D->CurveCount;
		CurvesFromCodePoint(D, D->Arena, Glyph, CodePoint, Font, &Glyph->CurveCount, &Glyph->Curves);
	}

	assert(CodePoint == ResultGlyph->CodePoint);

	return ResultGlyph;
}

d_atlas_glyph *D_GlyphFromCodePoint2(draw_context *D, u64 FontHash, d_font Font, u32 CodePoint)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	u64 Hash = M_FNVHashBytes(FontHash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	for(;Glyph->NextInHashSlot != Null && Glyph->Hash != Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *ResultGlyph = Glyph;

	if(Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		d_atlas_glyph *NextGlyph = ArenaPushArrayZero(D->Arena, d_atlas_glyph, 1);
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

		NextGlyph->CurveStart = D->CurveCount;
		CurvesFromCodePoint(D, D->Arena, NextGlyph,CodePoint, Font, &Glyph->CurveCount, &Glyph->Curves);
		ResultGlyph = NextGlyph;
	}
	else if(Glyph->Hash == 0)
	{
		d_atlas_glyph Zeroed = {0};
		Glyph[0] = Zeroed;
		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);

		Glyph->AdvanceX = AdvanceX;
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		stbtt_GetCodepointBox(&Font.Info, Glyph->CodePoint, &Glyph->Box.X, &Glyph->Box.Y,
															&Glyph->Box.Z, &Glyph->Box.W);
		PushGlyphOntoLRU(D, Glyph);

		Glyph->CurveStart = D->CurveCount;
		CurvesFromCodePoint(D, D->Arena, Glyph, CodePoint, Font, &Glyph->CurveCount, &Glyph->Curves);
		ResultGlyph = Glyph;
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
	Command.Vertices = ArenaPushArrayZero(Arena, r_vertex, 6);
	Command.VerticesCount = 6;

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

void D_Draw3DStr8(draw_context *D,
				m_arena *Arena,
				d_font Font,
				v3f Position,
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

	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));

	f32 StartX = Position.X;
	r_vertex *R_Vertices = Command.Vertices;

	b32 NewLine = False;
	v4f Colours[2] = {Colour, v4f(0, 0, 1, 1) };
	for(int I = 0; I < String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = String.Bytes[I];
		u32 NextCodePoint = String.Bytes[Min(String.Size, I + 1)];


		if(CodePoint == '\r') continue;
		
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
			if(NextCodePoint == '\r')
			{
				I += 1;
			}
			continue;
		}
		else
		{
			NewLine = False;
		}

		f32 LineHeight = (Font.Ascent - Font.Descent + Font.LineGap)*Scale;

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint2(D, Hash, Font, CodePoint);
		int Kern = stbtt_GetCodepointKernAdvance(&Font.Info, CodePoint, NextCodePoint);
		f32 AdvanceX = (Kern + Glyph->AdvanceX) * Scale * TabMultiplier;

		if(CodePoint == ' ')
		{
			v3f P = v3f(Position.X + 5.0, Position.Y + LineHeight/2.0 - 10.0, 0.0);
			D_DrawPointsArray(D, Arena, &P, 1, 10, v4f(0.1, 0.02, 0.0, 0.25));
		}

		if(String.Bytes[I] == '\t')
		{
			v3f P[2];
			P[0] = v3f(Position.X, Position.Y + LineHeight/2.0 - 10.0, 0.0);
			P[1] = P[0];
			P[1].X += (Kern + Glyph->AdvanceX) * Scale * TabMultiplier;
			D_DrawLinesArray(D, Arena, P, 2, 1.0, v4f(0.1, 0.02, 0.0, 0.25));
		}

		f32 GlyphWidth = ((f32)(Glyph->Box.Z - Glyph->Box.X)*Scale);
		f32 GlyphHeight = ((f32)(Glyph->Box.W - Glyph->Box.Y)*Scale);

		v2f Vertices[6] =
		{
			v2f(Position.X, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),

			v2f(Position.X, Position.Y),
			v2f(Position.X, Position.Y + GlyphHeight),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),
		};

		// v2f UVs[6] =
		// {

		// 	v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
		// 	v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
		// 	v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y),

		// 	v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
		// 	v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y),
		// 	v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y)
		// };
		
		v2f UVs[6] =
		{

			v2f(0.0f, 1.0f),
			v2f(1.0f, 1.0f),
			v2f(1.0f, 0.0f),

			v2f(0.0f, 1.0f),
			v2f(0.0f, 0.0f),
			v2f(1.0f, 0.0f)
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y + Glyph->Box.Y*Scale, Position.Z, 1),
				.UV = UVs[V],
				.Colour = Colours[NewLine],
				.CurveStart = Glyph->CurveStart,
				.CurveCount = Glyph->CurveCount
			};

			if(CodePoint != ' ') *R_Vertices++ = Vertex;
		}

		if(CodePoint == '\r' || CodePoint == '\n') continue;

		if(NewLine)
		{
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			continue;
		}

		// for(int CurveIndex = 0; CurveIndex < Glyph->CurveCount; CurveIndex += 3)
		// {
		// 	v2f Start = Glyph->Curves[CurveIndex + 0];
		// 	v2f Control = Glyph->Curves[CurveIndex + 1];
		// 	v2f End = Glyph->Curves[CurveIndex + 2];
		// 	v3f Line[3] = 
		// 	{
		// 		v3f(Position.X + Start.X*Scale, Position.Y - Start.Y*Scale, 0.0),
		// 		v3f(Position.X + Control.X*Scale, Position.Y - Control.Y*Scale, 0.0),
		// 		v3f(Position.X + End.X*Scale, Position.Y - End.Y*Scale, 0.0)
		// 	};
		// 	//D_DrawBezierArray(D, Arena, Line, 3, 2.0, Colour);
		// }

		Position.X += AdvanceX;
	}

	Command.Type = R_COMMAND_GLYPH;
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
	D_Draw3DStr8(D, Arena, Font, v3f(Position.X, Position.Y, 0.0), Colour, HeightInPixels, String);
}

void D_DrawSelectionRange(draw_context *D,
						  m_arena *Arena,
						  d_font Font,
						  v2f Position,
						  v4f Colour,
						  f32 HeightInPixels,
						  str8 String,
						  v2i Range,
						  s32 Start,
						  s32 End)
{
	if(Start == End) return;

	render_context *R = D->Render;

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);

	f32 StartX = Position.X;

	f32 SelectionStartX = StartX;

	b32 InRange = False;

	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&HeightInPixels, sizeof(f32));

	for(int I = Range.X; I < Range.Y; I++)
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
				d_atlas_glyph *Space  = D_GlyphFromCodePoint2(D, Hash, Font, ' ');
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


		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, CodePoint);

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
	Command.Vertices = ArenaPushArrayZero(Arena, r_vertex, 8);
	Command.VerticesCount = 8;

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
					  f32 PointSize,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushArrayZero(Arena, r_vertex, PositionsCount);
	Command.VerticesCount = PositionsCount;

	r_vertex *R_Vertices = Command.Vertices;

	for(int V = 0; V < PositionsCount; V++)
	{
		r_vertex Vertex = {
			.Position = v4f(Positions[V].X, Positions[V].Y, Positions[V].Z, 1),
			.Colour = Colour,
			.PointSize = PointSize,
		};

		*R_Vertices++ = Vertex;
	}
	Command.Type = R_COMMAND_POINTS;
	R_PushCommand(R, Arena, Command);
}

void D_DrawBezierArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  f32 Thickness,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushArrayZero(Arena, r_vertex, PositionsCount/3*6);
	Command.VerticesCount = 6*PositionsCount/3;
	r_vertex *R_Vertices = Command.Vertices;

	m44f Transform = R_CurrentTransform(D->Render);

	m44f Ortho = M44FOrthographic(0, D->ViewportSize.X, 0.0, D->ViewportSize.Y, -1000.0, 1000.0);
	R_PushTransform(D->Render, Ortho);

	for(int P = 0; P < PositionsCount; P += 3)
	{
		v3f A = Positions[P];
		v3f B = Positions[P + 1];
		v3f C = Positions[P + 2];

		v4f A4D = Mul(M44FTranspose(Transform), v4f(A.X, A.Y, A.Z, 1.0));
		A4D = Div(A4D, v4f(A4D.W, A4D.W, A4D.W, A4D.W));
		A = v3f(A4D.X, A4D.Y, A4D.Z);

		v4f B4D = Mul(M44FTranspose(Transform), v4f(B.X, B.Y, B.Z, 1.0));
		B4D = Div(B4D, v4f(B4D.W, B4D.W, B4D.W, B4D.W));
		B = v3f(B4D.X, B4D.Y, B4D.Z);

		v4f C4D = Mul(M44FTranspose(Transform), v4f(C.X, C.Y, C.Z, 1.0));
		C4D = Div(C4D, v4f(C4D.W, C4D.W, C4D.W, C4D.W));
		C = v3f(C4D.X, C4D.Y, C4D.Z);

		A.X = D->ViewportSize.X * (A.X*0.5 + 0.5);
		B.X = D->ViewportSize.X * (B.X*0.5 + 0.5); 
		C.X = D->ViewportSize.X * (C.X*0.5 + 0.5); 

		// We flip the NDC Y coordinate so that when we do the
		// SDF evaluation the [[position]]/fragCoord position aligns
		// with our coordinate system.
		// TODO(Oliver): explain this better
		A.Y = -A.Y;
		B.Y = -B.Y;
		C.Y = -C.Y;

		A.Y = D->ViewportSize.Y * (A.Y*0.5 + 0.5);
		B.Y = D->ViewportSize.Y * (B.Y*0.5 + 0.5);
		C.Y = D->ViewportSize.Y * (C.Y*0.5 + 0.5);

		
		

		v2f BL = v2f(Min(Min(A.X, B.X), C.X), Min(Min(A.Y, B.Y), C.Y));
		v2f TR = v2f(Max(Max(A.X, B.X), C.X), Max(Max(A.Y, B.Y), C.Y));
		BL.X -= Thickness + 5;
		BL.Y -= Thickness + 5;
		TR.X += Thickness + 5;
		TR.Y += Thickness + 5;
		f32 Width = fabs(TR.X - BL.X);
		f32 Height = fabs(TR.Y - BL.Y);
		v2f Vertices[6] =
		{
			v2f(BL.X, BL.Y),
			v2f(TR.X, BL.Y),
			v2f(TR.X, TR.Y),

			v2f(BL.X, BL.Y),
			v2f(BL.X, TR.Y),
			v2f(TR.X, TR.Y),
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y, A.Z, 1),
				.Colour = Colour,
				.Start = v2f(A.X, A.Y),
				.Control = v2f(B.X, B.Y),
				.End = v2f(C.X, C.Y),
				.Thickness = Thickness,
			};
			*R_Vertices++ = Vertex;
		}

	}
	Command.Type = R_COMMAND_BEZIER;
	R_PushCommand(R, Arena, Command);

	R_PopTransform(D->Render);
}

void D_DrawLinesArray(draw_context *D,
					  m_arena *Arena,
					  v3f *Positions,
					  int PositionsCount,
					  f32 Thickness,
					  v4f Colour)
{
	render_context *R = D->Render;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushArrayZero(Arena, r_vertex, PositionsCount/2*6);
	Command.VerticesCount = 6*PositionsCount/2;
	r_vertex *R_Vertices = Command.Vertices;

	m44f Transform = R_CurrentTransform(D->Render);

	

	m44f Ortho = M44FOrthographic(0, D->ViewportSize.X, D->ViewportSize.Y, 0.0, -1000.0, 1000.0);
	R_PushTransform(D->Render, Ortho);

	for(int P = 0; P < PositionsCount; P += 2)
	{
		v3f A = Positions[P];
		v3f B = Positions[P + 1];

		v4f A4D = Mul(M44FTranspose(Transform), v4f(A.X, A.Y, A.Z, 1.0));
		A4D = Div(A4D, v4f(A4D.W, A4D.W, A4D.W, A4D.W));
		A = v3f(A4D.X, A4D.Y, A4D.Z);

		v4f B4D = Mul(M44FTranspose(Transform), v4f(B.X, B.Y, B.Z, 1.0));
		B4D = Div(B4D, v4f(B4D.W, B4D.W, B4D.W, B4D.W));
		B = v3f(B4D.X, B4D.Y, B4D.Z);

		A.X = D->ViewportSize.X * (A.X*0.5 + 0.5);
		B.X = D->ViewportSize.X * (B.X*0.5 + 0.5); 

		// We flip the NDC Y coordinate so that when we do the
		// SDF evaluation the [[position]]/fragCoord position aligns
		// with our coordinate system.
		// TODO(Oliver): explain this better
		// A.Y = -A.Y;
		// B.Y = -B.Y;

		A.Y = D->ViewportSize.Y * (A.Y*0.5 + 0.5);
		B.Y = D->ViewportSize.Y * (B.Y*0.5 + 0.5);

		
		LogInfo("A: ?v2f B: ?v2f", A, B);

		v2f BL = v2f(Min(A.X, B.X), Min(A.Y, B.Y));
		v2f TR = v2f(Max(A.X, B.X), Max(A.Y, B.Y));
		BL.X -= Thickness + 5;
		BL.Y -= Thickness + 5;
		TR.X += Thickness + 5;
		TR.Y += Thickness + 5;
		f32 Width = fabs(TR.X - BL.X);
		f32 Height = fabs(TR.Y - BL.Y);
		v2f Vertices[6] =
		{
			v2f(BL.X, BL.Y),
			v2f(TR.X, BL.Y),
			v2f(TR.X, TR.Y),

			v2f(BL.X, BL.Y),
			v2f(BL.X, TR.Y),
			v2f(TR.X, TR.Y),
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y, A.Z, 1),
				.Colour = Colour,
				.Start = v2f(A.X, A.Y),
				.End = v2f(B.X, B.Y),
				.Thickness = Thickness
			};
			*R_Vertices++ = Vertex;
		}

	}
	Command.Type = R_COMMAND_LINES;
	R_PushCommand(R, Arena, Command);

	R_PopTransform(D->Render);
}


void D_DrawGrid(draw_context *Draw, m_arena *Arena, f32 Size, s32 Count)
{
	s32 LineCount = (Count + 1) * 2;
	v3f *Lines = (v3f*)ArenaPushArrayZero(Arena, v3f, LineCount * 2); // Each line has 2 vertices

	int Index = 0;

	f32 HalfSize = (Count * Size) / 2.0f;

	for (s32 I = 0; I <= Count; I++)
	{
		f32 Z = -HalfSize + (I * Size);
		Lines[Index++] = v3f(-HalfSize, -2, Z);
		Lines[Index++] = v3f(HalfSize,  -2, Z);
	}

	for (s32 I = 0; I <= Count; I++)
	{
		f32 X = -HalfSize + (I * Size);
		Lines[Index++] = v3f(X, -2, -HalfSize);
		Lines[Index++] = v3f(X, -2, HalfSize);
	}

	D_DrawLinesArray(Draw, Arena, Lines, LineCount * 2, 1.0, v4f(1.0, .05, 0, 1));
}

void D_DrawGrid2D(draw_context *Draw, m_arena *Arena, f32 Size, s32 Count)
{
	s32 LineCount = (Count + 1) * 2;
	v3f *Lines = (v3f*)ArenaPushArrayZero(Arena, v3f, LineCount * 2); // Each line has 2 vertices

	int Index = 0;

	f32 HalfSize = (Count * Size) / 2.0f;

	for (s32 I = 0; I <= Count; I++)
	{
		f32 Y = -HalfSize + (I * Size);
		Lines[Index++] = v3f(-HalfSize, Y, 0);
		Lines[Index++] = v3f(HalfSize,  Y, 0);
	}

	for (s32 I = 0; I <= Count; I++)
	{
		f32 X = -HalfSize + (I * Size);
		Lines[Index++] = v3f(X, -HalfSize, 0);
		Lines[Index++] = v3f(X, HalfSize, 0);
	}

	D_DrawLinesArray(Draw, Arena, Lines, LineCount * 2, 1.0, v4f(0.05, .0, 0, 1));
}
